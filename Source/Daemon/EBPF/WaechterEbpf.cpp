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

		static constexpr char const* EventNames[] = { "SocketCreate", "SocketConnect_4", "SocketConnect_6",
			"SocketBind_4", "SocketBind_6", "TCPSocketEstablished_4", "TCPSocketEstablished_6", "TCPSocketListening",
			"SocketAccept_4", "SocketAccept_6", "SocketClosed", "Traffic" };

		auto const  EventTypeIdx = static_cast<unsigned>(SocketEvent.EventType);
		auto const* EventName = EventTypeIdx < std::size(EventNames) ? EventNames[EventTypeIdx] : "Unknown";
#if WDEBUG
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
void WWaechterEbpf::PrePopulatePortToPid()
{
	if (!Data || !Data->PortToPid || !Data->PortToPid->IsValid())
	{
		spdlog::warn("port_to_pid map not available, skipping pre-population");
		return;
	}

	constexpr uint32_t TCP_LISTEN_STATE = 0x0A;

	// Step 1: Scan /proc/net/tcp and tcp6 for listening sockets → collect (port, inode)
	std::unordered_map<uint64_t, uint16_t> InodeToPort; // inode → local port

	auto ParseTcpProc = [&](std::string const& Path) {
		std::ifstream File(Path);
		if (!File.is_open())
			return;

		std::string Line;
		std::getline(File, Line); // skip header

		while (std::getline(File, Line))
		{
			std::istringstream Iss(Line);
			std::string        Slot, LocalAddr, RemAddr;
			uint32_t           State = 0;

			Iss >> Slot >> LocalAddr >> RemAddr >> std::hex >> State;

			if (State != TCP_LISTEN_STATE)
				continue;

			// Extract port from local address (format: ADDR:PORT in hex)
			auto ColonPos = LocalAddr.find(':');
			if (ColonPos == std::string::npos)
				continue;

			auto Port = static_cast<uint16_t>(std::stoul(LocalAddr.substr(ColonPos + 1), nullptr, 16));
			if (Port == 0)
				continue;

			// Skip remaining fields to get to inode (field index 9, 0-based)
			// Fields after state: tx_queue:rx_queue tr:tm->when retrnsmt uid timeout inode
			std::string TxRxQueue, TrTmWhen, Retrnsmt;
			uint32_t    Uid = 0, Timeout = 0;
			uint64_t    Inode = 0;
			Iss >> TxRxQueue >> TrTmWhen >> Retrnsmt >> Uid >> Timeout >> Inode;

			if (Inode != 0)
			{
				InodeToPort[Inode] = Port;
			}
		}
	};

	ParseTcpProc("/proc/net/tcp");
	ParseTcpProc("/proc/net/tcp6");

	if (InodeToPort.empty())
	{
		spdlog::debug("No listening TCP sockets found for port_to_pid pre-population");
		return;
	}

	// Step 2: Scan /proc/[pid]/fd to resolve inode → PID
	std::string const SocketPrefix = "socket:[";
	uint32_t          PopulatedCount = 0;

	DIR* ProcDir = opendir("/proc");
	if (!ProcDir)
	{
		spdlog::warn("Failed to open /proc for port_to_pid pre-population");
		return;
	}

	while (struct dirent* Entry = readdir(ProcDir))
	{
		// Only look at numeric directories (PIDs)
		if (Entry->d_type != DT_DIR)
			continue;

		char* End = nullptr;
		auto  Pid = static_cast<uint32_t>(strtoul(Entry->d_name, &End, 10));
		if (End == Entry->d_name || *End != '\0' || Pid == 0)
			continue;

		std::string FdPath = "/proc/" + std::string(Entry->d_name) + "/fd";
		DIR*        FdDir = opendir(FdPath.c_str());
		if (!FdDir)
			continue;

		while (struct dirent* FdEntry = readdir(FdDir))
		{
			std::string LinkPath = FdPath + "/" + FdEntry->d_name;
			char        Target[256];
			auto        Len = readlink(LinkPath.c_str(), Target, sizeof(Target) - 1);
			if (Len <= 0)
				continue;
			Target[Len] = '\0';

			std::string TargetStr(Target);
			if (TargetStr.compare(0, SocketPrefix.size(), SocketPrefix) != 0)
				continue;

			// Extract inode from "socket:[12345]"
			auto Inode = std::stoull(TargetStr.substr(SocketPrefix.size(), TargetStr.size() - SocketPrefix.size() - 1));

			if (auto It = InodeToPort.find(Inode); It != InodeToPort.end())
			{
				uint16_t Port = It->second;
				Data->PortToPid->Update(Port, Pid);
				spdlog::info("Pre-populated port_to_pid: port {} → PID {} (inode {})", Port, Pid, Inode);
				PopulatedCount++;
				InodeToPort.erase(It); // no need to look for this inode anymore
			}
		}
		closedir(FdDir);

		if (InodeToPort.empty())
			break; // all inodes resolved
	}
	closedir(ProcDir);

	spdlog::info("Pre-populated {} port_to_pid entries for pre-existing listening sockets", PopulatedCount);
}
