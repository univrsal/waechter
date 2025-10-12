
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

WWaechterEbpf::WWaechterEbpf(int InterfaceIndex, std::string const& ProgramObectFilePath)
	: WEbpfObj(ProgramObectFilePath), InterfaceIndex(InterfaceIndex)
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

	spdlog::error("{} ",  WErrnoUtil::StrError());
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
	// Data->UpdateData();

	WFlowStats fs{};
	uint32_t   gk = 0;
	auto Map = Data->GetGlobalStats();

	if (Map.Lookup(fs, gk	))
	{
		spdlog::info("{}|Global: {} bytes, {} pkts", gk, fs.Bytes, fs.Packets);
	}
}
