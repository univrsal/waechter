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

WWaechterEbpf::~WWaechterEbpf()
{
	if (RingBuf)
	{
		ring_buffer__free(RingBuf);
		RingBuf = nullptr;
	}
}

static const char* DirToStr(unsigned char d)
{
	switch (d) {
		case 1: return "egress";
		case 2: return "ingress";
		case 3: return "xdp";
		default: return "?";
	}
}

int WWaechterEbpf::OnIpEvent(void* ctx, void* data, size_t size)
{
	(void)ctx;
	if (size < sizeof(WIpEvent)) return 0;
	auto* ev = reinterpret_cast<WIpEvent*>(data);
	if (ev->Family == AF_INET)
	{
		spdlog::info("ip_event v4 dir={} proto={} s={:#x} d={:#x} sp={} dp={} pid={} cgid={} cookie={}",
			DirToStr(ev->Direction), ev->L4Proto, ev->V4.Saddr, ev->V4.Daddr, ev->V4.Sport, ev->V4.Dport,
			(unsigned)(ev->PidTgid >> 32), ev->CgroupId, ev->Cookie);
	}
	else if (ev->Family == AF_INET6)
	{
		spdlog::info("ip_event v6 dir={} proto={} sp={} dp={} pid={} cgid={} cookie={}",
			DirToStr(ev->Direction), ev->L4Proto, ev->V6.Sport, ev->V6.Dport,
			(unsigned)(ev->PidTgid >> 32), ev->CgroupId, ev->Cookie);
	}
	return 0;
}

int WWaechterEbpf::OnAcctEvent(void* ctx, void* data, size_t size)
{
	(void)ctx;
	if (size < sizeof(WAcctEvent)) return 0;
	auto* ev = reinterpret_cast<WAcctEvent*>(data);
	spdlog::trace("acct dir={} pid={} cgid={} cookie={} bytes={} pkts={} ", DirToStr(ev->Direction), ev->Pid, ev->CgroupId, ev->Cookie, ev->Bytes, ev->Packets);
	return 0;
}

int WWaechterEbpf::OnRlEvent(void* ctx, void* data, size_t size)
{
	(void)ctx;
	if (size < sizeof(WRlEvent)) return 0;
	auto* ev = reinterpret_cast<WRlEvent*>(data);
	spdlog::info("RL DROP pid={} cgid={} cookie={} bytes={} scope={:#x}", ev->Pid, ev->CgroupId, ev->Cookie, ev->Bytes, ev->ScopeMask);
	return 0;
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

	// Setup ring buffers (ip_events, acct_events, rl_events)
	int ip_fd   = this->FindMapFd("ip_events");
	int acct_fd = this->FindMapFd("acct_events");
	int rl_fd   = this->FindMapFd("rl_events");
	if (ip_fd < 0 || acct_fd < 0 || rl_fd < 0)
	{
		spdlog::critical("Missing ring buffer map(s): ip={} acct={} rl={}", ip_fd, acct_fd, rl_fd);
		return EEbpfInitResult::MAPS_NOT_FOUND;
	}

	RingBuf = ring_buffer__new(ip_fd, &WWaechterEbpf::OnIpEvent, this, nullptr);
	if (!RingBuf)
	{
		spdlog::critical("ring_buffer__new failed for ip_events");
		return EEbpfInitResult::LOAD_FAILED;
	}
	if (ring_buffer__add(RingBuf, acct_fd, &WWaechterEbpf::OnAcctEvent, this))
	{
		spdlog::critical("ring_buffer__add failed for acct_events");
		return EEbpfInitResult::LOAD_FAILED;
	}
	if (ring_buffer__add(RingBuf, rl_fd, &WWaechterEbpf::OnRlEvent, this))
	{
		spdlog::critical("ring_buffer__add failed for rl_events");
		return EEbpfInitResult::LOAD_FAILED;
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

int WWaechterEbpf::PollRingBuffers(int TimeoutMs)
{
	if (!RingBuf) return 0;
	int ret = ring_buffer__poll(RingBuf, TimeoutMs);
	if (ret < 0)
	{
		spdlog::warn("ring_buffer__poll failed: {}", ret);
	}
	return ret;
}
