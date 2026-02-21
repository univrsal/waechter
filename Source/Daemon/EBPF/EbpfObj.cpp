/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "EbpfObj.hpp"

#include <fcntl.h>
#include <bpf/bpf.h>
#include <net/if.h>
#include <sys/vfs.h>

#include "spdlog/spdlog.h"

#include "DaemonConfig.hpp"
#include "ErrnoUtil.hpp"
#include "Filesystem.hpp"
#include "NetworkInterface.hpp"

#ifndef CGROUP2_SUPER_MAGIC
	#define CGROUP2_SUPER_MAGIC 0x63677270
#endif

static bool IsCgroup2Mount(std::string const& Path)
{
	struct statfs Buf{};
	if (statfs(Path.c_str(), &Buf) != 0)
	{
		return false;
	}
	return static_cast<unsigned long>(Buf.f_type) == CGROUP2_SUPER_MAGIC;
}

WEbpfObj::WEbpfObj()
{
	int CGroupFdRaw = -1;
	if (!WDaemonConfig::GetInstance().CGroupPath.empty())
	{
		auto const& CGroupPath = WDaemonConfig::GetInstance().CGroupPath;

		if (!IsCgroup2Mount(CGroupPath))
		{
			spdlog::critical("Cgroup path '{}' is not a cgroup v2 mount. "
							 "BPF cgroup programs require a unified cgroup v2 hierarchy. "
							 "On systems using cgroup v1 or hybrid mode (common on Ubuntu), "
							 "you may need to boot with 'systemd.unified_cgroup_hierarchy=1' "
							 "or set the cgroup_path to a valid cgroup v2 mount point.",
				CGroupPath);
		}

		CGroupFdRaw = open(CGroupPath.c_str(), O_RDONLY | O_CLOEXEC);
		if (CGroupFdRaw < 0)
		{
			spdlog::critical("Failed to open cgroup path '{}': {}", CGroupPath, WErrnoUtil::StrError());
			return;
		}
		CGroupFd.reset(new int(CGroupFdRaw));
	}
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

bool WEbpfObj::CreateAndAttachTcxProgram(bpf_program* Program, int ifboverride)
{
	if (IfIndex == 0)
	{
		spdlog::critical("Cannot create TC hook: interface index is 0 (invalid)");
		return false;
	}

	bpf_tcx_opts Opts{};
	Opts.sz = sizeof(Opts);

	int If = ifboverride > 0 ? ifboverride : static_cast<int>(IfIndex);

	auto* Link = bpf_program__attach_tcx(Program, If, &Opts);
	if (!Link)
	{
		spdlog::critical("Link attachment for tcx program  failed: {}", WErrnoUtil::StrError());
		return false;
	}
	Links.emplace_back(Link, Program);
	return true;
}