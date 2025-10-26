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
#include "Net/PacketParser.hpp"
#include "EBPFCommon.h"

WWaechterEbpf::WWaechterEbpf(int InterfaceIndex, std::string const& ProgramObectFilePath)
	: WEbpfObj(ProgramObectFilePath), InterfaceIndex(InterfaceIndex)
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
		return EEbpfInitResult::OPEN_FAILED;
	}

	if (!this->Load())
	{
		spdlog::critical("Failed to load BPF file");
		return EEbpfInitResult::LOAD_FAILED;
	}

	if (!this->FindAndAttachXdpProgram("xdp_waechter", this->InterfaceIndex, XDP_FLAGS_UPDATE_IF_NOEXIST | XDP_FLAGS_SKB_MODE))
	{
		spdlog::critical("Failed to find and attach xdp_program");
		return EEbpfInitResult::XDP_ATTACH_FAILED;
	}

	if (!this->FindAndAttachProgram("cgskb_ingress", BPF_CGROUP_INET_INGRESS))
	{
		spdlog::critical("Failed to find and attach ingress");
		return EEbpfInitResult::CG_INGRESS_ATTACH_FAILED;
	}

	if (!this->FindAndAttachProgram("cgskb_egress", BPF_CGROUP_INET_EGRESS))
	{
		spdlog::critical("Failed to find and attach ingress");
		return EEbpfInitResult::CG_EGRESS_ATTACH_FAILED;
	}

	if (!this->FindAndAttachProgram("on_sock_create", BPF_CGROUP_INET_SOCK_CREATE))
	{
		spdlog::critical("Failed to attach on_sock_create (BPF_CGROUP_INET_SOCK_CREATE).");
		return EEbpfInitResult::INET_SOCKET_CREATE_FAILED;
	}

	if (!this->FindAndAttachProgram("on_connect4", BPF_CGROUP_INET4_CONNECT))
	{
		spdlog::critical("Failed to attach on_connect4 (BPF_CGROUP_INET4_CONNECT).");
		return EEbpfInitResult::INET4_SOCKET_CONNECT_FAILED;
	}

	if (!this->FindAndAttachProgram("on_connect6", BPF_CGROUP_INET6_CONNECT))
	{
		spdlog::critical("Failed to attach on_connect6 (BPF_CGROUP_INET6_CONNECT).");
		return EEbpfInitResult::INET6_SOCKET_CONNECT_FAILED;
	}

	Data = std::make_shared<WEbpfData>(*this);

	if (!Data->IsValid())
	{
		spdlog::critical("Failed to find one or more maps");
		return EEbpfInitResult::RING_BUFFERS_NOT_FOUND;
	}

	return EEbpfInitResult::SUCCESS;
}

void WWaechterEbpf::PrintStats()
{
	std::lock_guard Lock(Data->PacketData->GetDataMutex());
	auto&           PacketDataQueue = Data->PacketData->GetData();

	// Empty the deque
	while (!PacketDataQueue.empty())
	{
		auto& PacketData = PacketDataQueue.front();

		WPacketHeaderParser Parser;
		Parser.ParsePacket(PacketData.RawData, PACKET_HEADER_SIZE);
		if (!Parser.Dst.Address.IsLocalhost())
		{
			spdlog::info("Packet: cookie={} pid_tg_id={} cgroup_id={} direction={} bytes={} ts={} src={} dst={}",
				PacketData.Cookie,
				PacketData.PidTgId,
				PacketData.CGroupId,
				static_cast<int>(PacketData.Direction),
				PacketData.Bytes,
				PacketData.Timestamp,
				Parser.Src.to_string(),
				Parser.Dst.to_string());
		}
		PacketDataQueue.pop_front();
	}
}

void WWaechterEbpf::UpdateData()
{
	Data->UpdateData();
}
