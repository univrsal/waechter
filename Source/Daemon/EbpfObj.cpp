//
// Created by usr on 09/10/2025.
//

#include "EbpfObj.hpp"

#include <spdlog/spdlog.h>
#include <fcntl.h>
#include <bpf/bpf.h>
#include <net/if.h>

#include "DaemonConfig.hpp"
#include "ErrnoUtil.hpp"
#include "Filesystem.hpp"
#include "NetworkInterface.hpp"

WEbpfObj::WEbpfObj()
{
	int CGroupFdRaw = -1;
	if (!WDaemonConfig::GetInstance().CGroupPath.empty())
	{
		CGroupFdRaw = open(WDaemonConfig::GetInstance().CGroupPath.c_str(), O_RDONLY | O_CLOEXEC);
		if (CGroupFdRaw < 0)
		{
			spdlog::critical(
				"Failed to open cgroup path '{}': {}", WDaemonConfig::GetInstance().CGroupPath, WErrnoUtil::StrError());
			return;
		}
		CGroupFd.reset(new int(CGroupFdRaw));
	}
	// Get index of configured network interface
	IfIndex = WNetworkInterface::GetIfIndex(WDaemonConfig::GetInstance().NetworkInterfaceName);
}

WEbpfObj::~WEbpfObj()
{
	spdlog::info("Detaching eBPF programs...");
	for (auto const& Program : Programs)
	{
		int  ProgFd = bpf_program__fd(std::get<0>(Program));
		auto Type = std::get<1>(Program);
		if (ProgFd >= 0 && CGroupFd)
		{
			bpf_prog_detach2(ProgFd, *CGroupFd, Type);
		}
	}

	for (auto const& Link : Links)
	{
		if (bpf_link* BpfLink = std::get<0>(Link))
		{
			bpf_link__destroy(BpfLink);
		}
	}
}

bool WEbpfObj::FindAndAttachPlainProgram(std::string const& ProgName)
{
	auto* Prog = bpf_object__find_program_by_name(Obj, ProgName.c_str());

	if (!Prog)
	{
		spdlog::critical("Program '{}' not found", ProgName);
		return false;
	}

	bpf_link* Link = bpf_program__attach(Prog);

	if (!Link)
	{
		spdlog::critical("Link attachment for program '{}' failed: {}", ProgName, WErrnoUtil::StrError());
		return false;
	}

	Links.emplace_back(Link, Prog);

	return true;
}

bool WEbpfObj::FindAndAttachProgram(std::string const& ProgName, bpf_attach_type AttachType, unsigned int Flags)
{
	auto* Prog = bpf_object__find_program_by_name(Obj, ProgName.c_str());

	if (!Prog)
	{
		spdlog::critical("Program '{}' not found", ProgName);
		return false;
	}

	int ProgFd = bpf_program__fd(Prog);

	if (ProgFd < 0)
	{
		spdlog::critical("Program '{}' not open", ProgName);
		return false;
	}

	bpf_prog_detach2(ProgFd, *CGroupFd, AttachType);
	auto Result = bpf_prog_attach(bpf_program__fd(Prog), *CGroupFd, AttachType, Flags) == 0;
	if (Result)
	{
		Programs.emplace_back(Prog, AttachType);
	}
	return Result;
}

int WEbpfObj::FindMapFd(std::string const& MapFdPath) const
{
	return bpf_object__find_map_fd_by_name(this->Obj, MapFdPath.c_str());
}

bool WEbpfObj::CreateAndAttachTcxProgram(bpf_program* Program)
{
	if (IfIndex == 0)
	{
		spdlog::critical("Cannot create TC hook: interface index is 0 (invalid)");
		return false;
	}

	bpf_tcx_opts Opts{};
	Opts.sz = sizeof(Opts);

	auto* Link = bpf_program__attach_tcx(Program, static_cast<int>(IfIndex), &Opts);
	if (!Link)
	{
		spdlog::critical("Link attachment for tcx program  failed: {}", WErrnoUtil::StrError());
		return false;
	}
	Links.emplace_back(Link, Program);
	return true;
}