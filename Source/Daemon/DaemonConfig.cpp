//
// Created by usr on 09/10/2025.
//

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
	if (WFilesystem::Exists("./waechter.ini"))
	{
		Load("./waechter.ini");
	}
	else if (WFilesystem::Exists("/etc/waechter/waechter.ini"))
	{
		Load("/etc/waechter/waechter.ini");
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
	spdlog::info("cgroup path={}", CGroupPath);
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

	if (Reader.HasSection("network"))
	{
		SafeGet("network", "interface", NetworkInterfaceName);
		SafeGet("network", "cgroup_path", CGroupPath);

		SafeGet("daemon", "user", DaemonUser);
	}
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
		spdlog::critical("Your kernel does not seem to support BTF, which is required for the EBPF program to work: failed to open /sys/kernel/btf/vmlinux: {}", WErrnoUtil::StrError());
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
