//
// Created by usr on 12/10/2025.
//

#include "WaechterEbpf.hpp"

#include <bpf/bpf.h>
#include <linux/if_link.h>
#include <net/if.h>
#include <spdlog/spdlog.h>

#include "DaemonConfig.hpp"
#include "EbpfData.hpp"
#include "EBPFCommon.h"
#include "Types.hpp"
#include "Format.hpp"
#include "Data/SystemMap.hpp"

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

	if (!this->FindAndAttachProgram("cgskb_ingress", BPF_CGROUP_INET_INGRESS))
	{
		spdlog::critical("Failed to find and attach ingress");
		return EEbpfInitResult::Attach_Failed;
	}

	if (!this->FindAndAttachProgram("cgskb_egress", BPF_CGROUP_INET_EGRESS))
	{
		spdlog::critical("Failed to find and attach ingress");
		return EEbpfInitResult::Attach_Failed;
	}

	if (!this->FindAndAttachProgram("on_sock_create", BPF_CGROUP_INET_SOCK_CREATE))
	{
		spdlog::critical("Failed to attach on_sock_create (BPF_CGROUP_INET_SOCK_CREATE).");
		return EEbpfInitResult::Attach_Failed;
	}

	if (!this->FindAndAttachProgram("on_connect4", BPF_CGROUP_INET4_CONNECT))
	{
		spdlog::critical("Failed to attach on_connect4 (BPF_CGROUP_INET4_CONNECT).");
		return EEbpfInitResult::Attach_Failed;
	}

	if (!this->FindAndAttachProgram("on_connect6", BPF_CGROUP_INET6_CONNECT))
	{
		spdlog::critical("Failed to attach on_connect6 (BPF_CGROUP_INET6_CONNECT).");
		return EEbpfInitResult::Attach_Failed;
	}

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

		/*
		 This will also create the application/process/socket entries as needed
		 NE_Traffic and NE_SocketClose usually have PID set to 0, so for those to be properly associated with a process,
		 the daemon has to first capture the socket creation and connection events for that socket cookie.
		 So for traffic events we fail silently if no matching socket is found because it usually just means
		 we weren't around to capture the socket creation/connection.
		*/
		auto const bSilentFail = SocketEvent.EventType == NE_Traffic || SocketEvent.EventType == NE_SocketClosed;
		auto       SocketInfo = WSystemMap::GetInstance().MapSocket(SocketEvent.Cookie, Tgid, bSilentFail);

		switch (SocketEvent.EventType)
		{
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
					SocketInfo->ProcessSocketEvent(SocketEvent);
				}
				break;
			case NE_Traffic:
				if (SocketEvent.Data.TrafficEventData.Direction == PD_Incoming)
				{
					WSystemMap::GetInstance().PushIncomingTraffic(
						SocketEvent.Data.TrafficEventData.Bytes, SocketEvent.Cookie);
				}
				else if (SocketEvent.Data.TrafficEventData.Direction == PD_Outgoing)
				{
					WSystemMap::GetInstance().PushOutgoingTraffic(
						SocketEvent.Data.TrafficEventData.Bytes, SocketEvent.Cookie);
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
