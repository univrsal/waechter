/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "DaemonConfig.hpp"

#include <sys/resource.h>
#include <INIReader.h>
#include <spdlog/spdlog.h>
#include <pwd.h>
#include <fcntl.h>

#include "ErrnoUtil.hpp"
#include "Filesystem.hpp"
#include "NetworkInterface.hpp"

WDaemonConfig::WDaemonConfig()
{
	SetDefaults();
	if (WFilesystem::Exists("./waechterd.ini"))
	{
		Load("./waechterd.ini");
	}
	else if (WFilesystem::Exists("/etc/waechter/waechterd.ini"))
	{
		Load("/etc/waechter/waechterd.ini");
	}
	else
	{
		spdlog::info("no configuration file found, using defaults");
	}
	BumpMemlockRlimit();
}

void WDaemonConfig::LogConfig()
{
	spdlog::info("network interface={}", NetworkInterfaceName);
	if (NetworkInterfaceName != IngressNetworkInterfaceName && !IngressNetworkInterfaceName.empty())
	{
		spdlog::info("ingress network interface={}", IngressNetworkInterfaceName);
	}
	spdlog::info("cgroup path={}", CGroupPath);
	spdlog::info("socket path={}", DaemonSocketPath);
}

void WDaemonConfig::Load(std::string const& Path)
{
	INIReader Reader(Path);

	auto SafeGet = [&](std::string const& Section, std::string const& Name, std::string& OutVal) {
		if (Reader.HasValue(Section, Name))
		{
			OutVal = Reader.Get(Section, Name, OutVal);
		}
	};

	if (Reader.ParseError() < 0)
	{
		spdlog::error("can't load '{}': {}", Path, Reader.ParseErrorMessage());
		return;
	}

	SafeGet("network", "interface", NetworkInterfaceName);
	SafeGet("network", "ingress_interface", IngressNetworkInterfaceName);
	SafeGet("network", "cgroup_path", CGroupPath);

	if (NetworkInterfaceName == "auto")
	{
		// Auto-select the first non-loopback interface
		auto Ifaces = WNetworkInterface::list();
		for (auto const& Iface : Ifaces)
		{
			if (Iface != "lo")
			{
				NetworkInterfaceName = Iface;
				break;
			}
		}
	}

	if (IngressNetworkInterfaceName == "auto")
	{
		IngressNetworkInterfaceName = NetworkInterfaceName;
	}

	SafeGet("daemon", "user", DaemonUser);
	SafeGet("daemon", "group", DaemonGroup);
	SafeGet("daemon", "socket_path", DaemonSocketPath);
	SafeGet("daemon", "ip_proc_socket_path", IpLinkProcSocketPath);
	DaemonSocketMode = static_cast<mode_t>(Reader.GetInteger("daemon", "socket_permissions", DaemonSocketMode));
	SafeGet("daemon", "ebpf_program_object_path", EbpfProgramObjectPath);
}

void WDaemonConfig::SetDefaults()
{
	auto Ifaces = WNetworkInterface::list();
	if (Ifaces.size() > 1)
	{
		NetworkInterfaceName = Ifaces[1];
	}
	else if (Ifaces.size() == 1)
	{
		NetworkInterfaceName = Ifaces[0];
	}
	else
	{
		NetworkInterfaceName = "eth0";
	}
	IngressNetworkInterfaceName = NetworkInterfaceName;
}

void WDaemonConfig::BumpMemlockRlimit()
{
	spdlog::info("bumping memlock rlimit");
	rlimit r = { RLIM_INFINITY, RLIM_INFINITY };
	setrlimit(RLIMIT_MEMLOCK, &r);
}

void WDaemonConfig::BTFTest()
{
	int Fd = open("/sys/kernel/btf/vmlinux", O_RDONLY | O_CLOEXEC);
	if (Fd < 0)
	{
		spdlog::critical(
			"Your kernel does not seem to support BTF, which is required for the EBPF program to work: failed to open /sys/kernel/btf/vmlinux: {}",
			WErrnoUtil::StrError());
	}
	else
	{
		spdlog::info("BTF is supported by the kernel");
	}
}

bool WDaemonConfig::DropPrivileges()
{
	passwd* PW = getpwnam(DaemonUser.c_str());
	if (!PW)
	{
		spdlog::critical("User {} not found", DaemonUser);
		return false;
	}

	if (setgid(PW->pw_gid) != 0 || setuid(PW->pw_uid) != 0)
	{
		spdlog::critical("Failed to drop privileges: {}", WErrnoUtil::StrError());
		return false;
	}

	return true;
}
