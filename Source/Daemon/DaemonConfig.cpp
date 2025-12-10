//
// Created by usr on 09/10/2025.
//

#include "DaemonConfig.hpp"

#include <sys/resource.h>
#include <INIReader.h>
#include <spdlog/spdlog.h>
#include <pwd.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/capability.h>

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

	SafeGet("daemon", "user", DaemonUser);
	SafeGet("daemon", "group", DaemonGroup);
	SafeGet("daemon", "socket_path", DaemonSocketPath);
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
	if (prctl(PR_SET_KEEPCAPS, 1L, 0L, 0L, 0L) != 0)
	{
		spdlog::critical("Failed to set PR_SET_KEEPCAPS: {}", WErrnoUtil::StrError());
		return false;
	}

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

	cap_t       Caps = cap_init();
	cap_value_t CapsList[1] = { CAP_NET_ADMIN };

	// Set Permitted and Effective sets
	if (cap_set_flag(Caps, CAP_EFFECTIVE, 1, CapsList, CAP_SET) == -1
		|| cap_set_flag(Caps, CAP_PERMITTED, 1, CapsList, CAP_SET) == -1)
	{
		spdlog::critical("Failed to set capabilities flags: {}", WErrnoUtil::StrError());
		cap_free(Caps);
		return false;
	}

	// Apply the capabilities
	if (cap_set_proc(Caps) == -1)
	{
		spdlog::critical("Failed to set capabilities: {}", WErrnoUtil::StrError());
		cap_free(Caps);
		return false;
	}
	cap_free(Caps);

	return true;
}
