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
#include "Data/SocketInfo.hpp"

WWaechterEbpf::WWaechterEbpf(std::string const& ProgramObectFilePath_)
	: WEbpfObj(ProgramObectFilePath_)
{
}

WWaechterEbpf::~WWaechterEbpf()
{
}

EEbpfInitResult WWaechterEbpf::Init()
{
	if (this->InterfaceIndex < 0)
	{
		this->InterfaceIndex = static_cast<int>(if_nametoindex(WDaemonConfig::GetInstance().NetworkInterfaceName.c_str()));
	}

	if (!this->Obj)
	{
		spdlog::critical("Failed to open BPF object file");
		return EEbpfInitResult::Open_Failed;
	}

	if (!this->Load())
	{
		spdlog::critical("Failed to load BPF file");
		return EEbpfInitResult::Load_Failed;
	}

	if (!this->FindAndAttachXdpProgram("xdp_waechter", this->InterfaceIndex, XDP_FLAGS_UPDATE_IF_NOEXIST | XDP_FLAGS_SKB_MODE))
	{
		spdlog::critical("Failed to find and attach xdp_program");
		return EEbpfInitResult::Xdp_Attach_Failed;
	}

	if (!this->FindAndAttachProgram("cgskb_ingress", BPF_CGROUP_INET_INGRESS))
	{
		spdlog::critical("Failed to find and attach ingress");
		return EEbpfInitResult::Cg_Ingress_Attach_Failed;
	}

	if (!this->FindAndAttachProgram("cgskb_egress", BPF_CGROUP_INET_EGRESS))
	{
		spdlog::critical("Failed to find and attach ingress");
		return EEbpfInitResult::CG_Egress_Attach_Failed;
	}

	if (!this->FindAndAttachProgram("on_sock_create", BPF_CGROUP_INET_SOCK_CREATE))
	{
		spdlog::critical("Failed to attach on_sock_create (BPF_CGROUP_INET_SOCK_CREATE).");
		return EEbpfInitResult::Inet_Socket_Create_Failed;
	}

	if (!this->FindAndAttachProgram("on_connect4", BPF_CGROUP_INET4_CONNECT))
	{
		spdlog::critical("Failed to attach on_connect4 (BPF_CGROUP_INET4_CONNECT).");
		return EEbpfInitResult::Inet4_Socket_Connect_Failed;
	}

	if (!this->FindAndAttachProgram("on_connect6", BPF_CGROUP_INET6_CONNECT))
	{
		spdlog::critical("Failed to attach on_connect6 (BPF_CGROUP_INET6_CONNECT).");
		return EEbpfInitResult::Inet6_Socket_Connect_Failed;
	}

	if (!this->FindAndAttachPlainProgram("on_tcp_set_state"))
	{
		spdlog::critical("Failed to attach on_tcp_set_state.");
		return EEbpfInitResult::On_Tcp_Set_State_Failed;
	}

	if (!this->FindAndAttachPlainProgram("on_inet_sock_destruct"))
	{
		spdlog::critical("Failed to attach on_inet_sock_destruct.");
		return EEbpfInitResult::On_Inet_Sock_Destruct_Failed;
	}

	Data = std::make_shared<WEbpfData>(*this);

	if (!Data->IsValid())
	{
		spdlog::critical("Failed to find one or more maps");
		return EEbpfInitResult::Ring_Buffers_Not_Found;
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
		spdlog::error("Queue for socket events is filling up, polling rate might be too low. {} events waiting", SocketEventQueue.size());
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

		if (SocketInfo)
		{
			switch (SocketEvent.EventType)
			{
				case NE_SocketCreate:
					SocketInfo->SocketState = ESocketConnectionState::Created;
					break;
				case NE_SocketConnect_4:
				case NE_SocketConnect_6:
					SocketInfo->ProcessSocketEvent(SocketEvent);
					break;
				case NE_Traffic:
					if (SocketEvent.Data.TrafficEventData.Direction == PD_Incoming)
					{
						WSystemMap::GetInstance().PushIncomingTraffic(SocketEvent.Data.TrafficEventData.Bytes, SocketEvent.Cookie);
					}
					else if (SocketEvent.Data.TrafficEventData.Direction == PD_Outgoing)
					{
						WSystemMap::GetInstance().PushOutgoingTraffic(SocketEvent.Data.TrafficEventData.Bytes, SocketEvent.Cookie);
					}
					break;
				case NE_SocketClosed:
					WSystemMap::GetInstance().MarkSocketForRemoval(SocketEvent.Cookie);
					break;
				default:;
			}
		}
		SocketEventQueue.pop_front();
	}
}
