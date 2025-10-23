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

	Data = std::make_shared<WEbpfData>(*this);

	if (!Data->IsValid())
	{
		spdlog::critical("Failed to find one or more maps");
		return EEbpfInitResult::MAPS_NOT_FOUND;
	}


	return EEbpfInitResult::SUCCESS;
}

void WWaechterEbpf::PrintStats()
{
	WPacketData PacketData{};
	uint32_t Key = 0;
	if (Data->PacketStatsMap.Lookup(PacketData, Key))
	{
		// Print out the entire packet data as hex
		std::string HexData;
		for (size_t i = 0; i < sizeof(PacketData.RawData); ++i)
		{
			char buf[3];
			snprintf(buf, sizeof(buf), "%02x", PacketData.RawData[i]);
			HexData += buf;
		}
		PacketKey ParsedPacket{};
		if (parse_packet_l3(PacketData.RawData, sizeof(PacketData.RawData), ParsedPacket))
		{
			spdlog::info("Packet Parsed (Key={}): Family={}, L4Proto={}, {}_{}, {}_{}",
			             Key,
			             ParsedPacket.family == EIPFamily::IPv4 ? "IPv4" : "IPv6",
			             static_cast<int>(ParsedPacket.l4_proto),
			             ParsedPacket.src_to_string(),
			             ParsedPacket.src_port,
			             ParsedPacket.dst_to_string(),
			             ParsedPacket.dst_port);
		}
		else
		{
			spdlog::info("Packet Parsing Failed (Key={})", Key);
		}
		// spdlog::info("Packet Data (Key={}): {}", Key, HexData);
	}
}

int WWaechterEbpf::PollRingBuffers(int)
{
	return 0;
}
