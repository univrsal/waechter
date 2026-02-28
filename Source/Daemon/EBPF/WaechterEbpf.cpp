/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "WaechterEbpf.hpp"

#include <bpf/bpf.h>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <unordered_map>

#include "spdlog/spdlog.h"
#include "tracy/Tracy.hpp"

#include "EbpfData.hpp"
#include "EBPFCommon.h"
#include "Types.hpp"
#include "Format.hpp"
#include "NetworkInterface.hpp"
#include "Data/SystemMap.hpp"
#include "Net/IPLink.hpp"

WWaechterEbpf::WWaechterEbpf() = default;

WWaechterEbpf::~WWaechterEbpf()
{
	waechter_ebpf__destroy(Skeleton);
}

EEbpfInitResult WWaechterEbpf::Init()
{
	Skeleton = waechter_ebpf__open();
	if (!Skeleton)
	{
		spdlog::error("Failed to open eBPF object");
		return EEbpfInitResult::Open_Failed;
	}

	Skeleton->rodata->IngressInterfaceId = static_cast<int>(WIPLink::GetInstance().WaechterIngressIfIndex);
	Obj = Skeleton->obj;

	auto Result = waechter_ebpf__load(Skeleton);

	if (Result != 0)
	{
		spdlog::error("Failed to load eBPF object: {}", Result);
		return EEbpfInitResult::Load_Failed;
	}

	Result = waechter_ebpf__attach(Skeleton);

	if (Result != 0)
	{
		spdlog::error("Failed to attach eBPF programs: {}", Result);
		return EEbpfInitResult::Attach_Failed;
	}

	Data = std::make_shared<WEbpfData>(*this);

	if (!FindAndAttachProgram("cgskb_ingress", BPF_CGROUP_INET_INGRESS))
	{
		spdlog::critical("Failed to find and attach ingress");
		return EEbpfInitResult::Attach_Failed;
	}

	if (!FindAndAttachProgram("cgskb_egress", BPF_CGROUP_INET_EGRESS))
	{
		spdlog::critical("Failed to find and attach ingress");
		return EEbpfInitResult::Attach_Failed;
	}

	if (!FindAndAttachProgram("on_sock_create", BPF_CGROUP_INET_SOCK_CREATE))
	{
		spdlog::critical("Failed to attach on_sock_create (BPF_CGROUP_INET_SOCK_CREATE).");
		return EEbpfInitResult::Attach_Failed;
	}

	if (!FindAndAttachProgram("on_connect4", BPF_CGROUP_INET4_CONNECT))
	{
		spdlog::critical("Failed to attach on_connect4 (BPF_CGROUP_INET4_CONNECT).");
		return EEbpfInitResult::Attach_Failed;
	}

	if (!FindAndAttachProgram("on_connect6", BPF_CGROUP_INET6_CONNECT))
	{
		spdlog::critical("Failed to attach on_connect6 (BPF_CGROUP_INET6_CONNECT).");
		return EEbpfInitResult::Attach_Failed;
	}

	if (!CreateAndAttachTcxProgram(Skeleton->progs.cls_egress))
	{
		spdlog::critical("Failed to create and attach egress tcx program.");
		return EEbpfInitResult::Attach_Failed;
	}

	auto const IfbIndex = static_cast<int>(WNetworkInterface::GetIfIndex(WIPLink::IfbDev));
	if (!CreateAndAttachTcxProgram(Skeleton->progs.ifb_cls_egress, IfbIndex))
	{
		spdlog::critical("Failed to create and attach egress tcx program.");
		return EEbpfInitResult::Attach_Failed;
	}

	PrePopulatePortToPid();

	return EEbpfInitResult::Success;
}

void WWaechterEbpf::PrintStats()
{
	spdlog::info("System Traffic: Download Speed: {}, Upload Speed: {}",
		WTrafficFormat::AutoFormat(WSystemMap::GetInstance().GetDownloadSpeed()),
		WTrafficFormat::AutoFormat(WSystemMap::GetInstance().GetUploadSpeed()));
}

void WWaechterEbpf::UpdateData()
{
	Data->UpdateData();
	std::lock_guard Lock(Data->SocketEvents->GetDataMutex());
	auto&           SocketEventQueue = Data->SocketEvents->GetData();

	if (SocketEventQueue.size() > 100)
	{
		if (QueuePileupStartTime == 0)
		{
			QueuePileupStartTime = WTime::GetEpochMs();
		}
		else if (WTime::GetEpochMs() - QueuePileupStartTime > 5000)
		{
			spdlog::warn(
				"Queue for socket events has been filling up for more than 5 seconds, polling rate might be too low. {} events waiting",
				SocketEventQueue.size());
			QueuePileupStartTime = WTime::GetEpochMs(); // reset timer to avoid spamming logs
		}
	}
	else
	{
		QueuePileupStartTime = 0;
	}

	while (!SocketEventQueue.empty())
	{
		auto& SocketEvent = SocketEventQueue.front();

		// extract the PID
		uint64_t Raw = SocketEvent.PidTgId;
		auto     Tgid = static_cast<WProcessId>(Raw >> 32);

#if WDEBUG

		static constexpr char const* EventNames[] = { "SocketCreate", "SocketConnect_4", "SocketConnect_6",
			"SocketBind_4", "SocketBind_6", "TCPSocketEstablished_4", "TCPSocketEstablished_6", "TCPSocketListening",
			"SocketAccept_4", "SocketAccept_6", "SocketClosed", "Traffic" };
		auto const  EventTypeIdx = static_cast<unsigned>(SocketEvent.EventType);
		auto const* EventName = EventTypeIdx < std::size(EventNames) ? EventNames[EventTypeIdx] : "Unknown";
		if (SocketEvent.EventType != NE_Traffic)
		{
			spdlog::debug("[eBPF event] type={} cookie={} pid={}", EventName, SocketEvent.Cookie, Tgid);
		}

		if (SocketEvent.EventType == NE_TCPSocketEstablished_4 || SocketEvent.EventType == NE_TCPSocketEstablished_6)
		{
			spdlog::debug("[eBPF TCP_ESTABLISHED] cookie={} pid={} localPort={} remotePort={} isAccept={}",
				SocketEvent.Cookie, Tgid, SocketEvent.Data.TCPSocketEstablishedEventData.UserPort,
				SocketEvent.Data.TCPSocketEstablishedEventData.RemotePort,
				SocketEvent.Data.TCPSocketEstablishedEventData.bIsAccept);
		}

		if (SocketEvent.EventType == NE_SocketAccept_4 || SocketEvent.EventType == NE_SocketAccept_6)
		{
			spdlog::debug("[eBPF SocketAccept] cookie={} pid={} srcPort={} dstPort={}", SocketEvent.Cookie, Tgid,
				SocketEvent.Data.SocketAcceptEventData.SourcePort,
				SocketEvent.Data.SocketAcceptEventData.DestinationPort);
		}
#endif

		/*
		 This will also create the application/process/socket entries as needed
		 NE_Traffic and NE_SocketClose usually have PID set to 0, so for those to be properly associated with a process,
		 the daemon has to first capture the socket creation and connection events for that socket cookie.
		 So for traffic events we fail silently if no matching socket is found because it usually just means
		 we weren't around to capture the socket creation/connection.
		*/
		auto const bSilentFail = SocketEvent.EventType == NE_Traffic || SocketEvent.EventType == NE_SocketClosed
			|| SocketEvent.EventType == NE_TCPSocketEstablished_4 || SocketEvent.EventType == NE_TCPSocketEstablished_6;
		auto SocketInfo = WSystemMap::GetInstance().MapSocket(SocketEvent, Tgid, bSilentFail);

		switch (SocketEvent.EventType)
		{
			case NE_SocketAccept_4:
			case NE_SocketAccept_6:
			case NE_TCPSocketListening:
			case NE_SocketBind_4:
			case NE_SocketBind_6:
			case NE_SocketCreate:
			case NE_SocketConnect_4:
			case NE_SocketConnect_6:
			case NE_TCPSocketEstablished_4:
			case NE_TCPSocketEstablished_6:
				if (SocketInfo)
				{
					ZoneScopedN("ProcessSocketEvent");
					SocketInfo->ProcessSocketEvent(SocketEvent);
				}
				break;
			case NE_Traffic:
				if (SocketEvent.Data.TrafficEventData.Direction == PD_Incoming)
				{
					ZoneScopedN("PushIncomingTraffic");
					WSystemMap::GetInstance().PushIncomingTraffic(SocketEvent);
				}
				else if (SocketEvent.Data.TrafficEventData.Direction == PD_Outgoing)
				{
					ZoneScopedN("PushOutgoingTraffic");
					WSystemMap::GetInstance().PushOutgoingTraffic(SocketEvent);
				}
				break;
			case NE_SocketClosed:
				WSystemMap::GetInstance().MarkSocketForRemoval(SocketEvent.Cookie);
				break;
			default:;
		}
		SocketEventQueue.pop_front();
	}
}

// Scan /proc/net/tcp[6] for listening ports and their socket inodes,
// then resolve inode → PID by scanning /proc/[pid]/fd links.
// Populates the port_to_pid eBPF map so sock_graft can correctly
// attribute accepted connections to pre-existing server processes.
void WWaechterEbpf::PrePopulatePortToPid() const
{
	if (!Data || !Data->PortToPid || !Data->PortToPid->IsValid())
	{
		spdlog::warn("port_to_pid map not available, skipping pre-population");
		return;
	}

	WSocketStateParser const Parser{};
	for (auto const& ListenSocket : Parser.GetListeningSockets())
	{
		if (Data->PortToPid->Update(ListenSocket.LocalEndpoint.Port, static_cast<uint32_t>(ListenSocket.PID)))
		{
			spdlog::debug(
				"Pre-populated port_to_pid: port {} → PID {}", ListenSocket.LocalEndpoint.ToString(), ListenSocket.PID);
		}
		else
		{
			spdlog::warn("Failed to pre-populate port_to_pid for port {} PID {}", ListenSocket.LocalEndpoint.ToString(),
				ListenSocket.PID);
		}
	}

	spdlog::info(
		"Pre-populated {} port_to_pid entries for pre-existing listening sockets", Parser.GetListeningSockets().size());
}
