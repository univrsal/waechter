//
// Created by usr on 09/10/2025.
//

#include "EbpfObj.hpp"
#include "DaemonConfig.hpp"
#include "ErrnoUtil.hpp"
#include "Filesystem.hpp"

#include <spdlog/spdlog.h>
#include <fcntl.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>

WEbpfObj::WEbpfObj(std::string ProgramObectFilePath)
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

	Obj = bpf_object__open_file(ProgramObectFilePath.c_str(), nullptr);
	errno = 0;
}

WEbpfObj::~WEbpfObj()
{
	for (auto const& Program : Programs)
	{
		int  ProgFd = bpf_program__fd(std::get<0>(Program));
		auto Type = std::get<1>(Program);
		if (ProgFd >= 0 && CGroupFd)
		{
			if (Type == BPF_XDP)
			{
				bpf_xdp_detach(XdpIfIndex, 0, nullptr);
			}
			else
			{
				bpf_prog_detach2(ProgFd, *CGroupFd, Type);
			}
		}
	}

	for (auto const& Link : Links)
	{
		if (bpf_link* BpfLink = std::get<0>(Link))
		{
			bpf_link__destroy(BpfLink);
		}
	}

	if (Obj)
	{
		bpf_object__close(Obj);
	}
}

bool WEbpfObj::Load()
{
	if (!Obj)
	{
		return false;
	}
	return bpf_object__load(Obj) == 0;
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

bool WEbpfObj::FindAndAttachXdpProgram(std::string const& ProgName, int IfIndex, unsigned int Flags)
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

	bpf_xdp_detach(IfIndex, Flags, nullptr);
	auto Result = bpf_xdp_attach(IfIndex, ProgFd, Flags, nullptr);

	if (Result)
	{
		Programs.emplace_back(Prog, BPF_XDP);
	}

	XdpIfIndex = IfIndex;
	return Result == 0;
}

int WEbpfObj::FindMapFd(std::string const& MapFdPath) const
{
	return bpf_object__find_map_fd_by_name(this->Obj, MapFdPath.c_str());
}