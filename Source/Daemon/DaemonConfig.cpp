/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "DaemonConfig.hpp"

#include <sys/resource.h>
#include <pwd.h>
#include <fcntl.h>

#include "mini.hpp"
#include "spdlog/spdlog.h"

#include "ErrnoUtil.hpp"
#include "Filesystem.hpp"
#include "NetworkInterface.hpp"
#include "Random.hpp"

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
		ConfigPath = "";
		spdlog::info("no configuration file found, using defaults");
		bFirstTimeSetupRun = true; // don't want this to show the setup guide every time a user connects
	}
	BumpMemlockRlimit();
	if (!Write())
	{
		bFirstTimeSetupRun = true;
		spdlog::warn("Failed to write configuration file {}", ConfigPath);
	}
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
	mINI::INIFile const File(Path);
	mINI::INIStructure  Ini;

	if (!File.read(Ini))
	{
		spdlog::error("Failed to read configuration file {}", Path);
		return;
	}

	auto SafeGet = [&](std::string const& Section, std::string const& Name, std::string& OutVal) {
		if (Ini.has(Section) && Ini[Section].has(Name))
		{
			OutVal = Ini[Section][Name];
		}
	};
	auto SafeGetInt = [&](std::string const& Section, std::string const& Name, int& OutVal) {
		std::string StrVal;
		SafeGet(Section, Name, StrVal);
		if (!StrVal.empty())
		{
			try
			{
				OutVal = std::stoi(StrVal);
			}
			catch (std::exception const& e)
			{
			}
		}
	};

	SafeGet("network", "interface", NetworkInterfaceName);
	SafeGet("network", "ingress_interface", IngressNetworkInterfaceName);
	SafeGet("network", "cgroup_path", CGroupPath);

	if (NetworkInterfaceName == "auto")
	{
		// Auto-select the first non-loopback interface
		auto const Ifaces = WNetworkInterface::List();
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
	SafeGet("daemon", "websocket_auth_token", WebSocketAuthToken);
	SafeGetBool("daemon", "first_time_setup_run", bFirstTimeSetupRun);

	int SocketMode{ static_cast<int>(DaemonSocketMode) };
	SafeGetInt("daemon", "socket_permissions", SocketMode);
	DaemonSocketMode = static_cast<mode_t>(SocketMode);

	if (WebSocketAuthToken.empty() || WebSocketAuthToken == "change_me")
	{
		WebSocketAuthToken = WRandom::GenerateRandomHexString(24);
		spdlog::warn("No WebSocket auth token specified in config generated random token: {}", WebSocketAuthToken);
	}
	std::string IgnoredConnectionHistoryAppNamesStr{};
	std::string IgnoredConnectionHistoryPortsStr{};
	SafeGet("daemon", "ignored_connection_history_app_names", IgnoredConnectionHistoryAppNamesStr);
	SafeGet("daemon", "ignored_connection_history_remote_ports", IgnoredConnectionHistoryAppNamesStr);

	IgnoredConnectionHistoryApps = WStringFormat::SplitString(IgnoredConnectionHistoryAppNamesStr, ';');
	for (auto const& PortStr : WStringFormat::SplitString(IgnoredConnectionHistoryPortsStr, ';'))
	{
		try
		{
			auto Port = static_cast<uint16_t>(std::stoul(PortStr));
			IgnoredConnectionHistoryPorts.push_back(Port);
		}
		catch (std::exception const& e)
		{
			spdlog::error("Invalid port '{}' in ignored_connection_history_remote_ports: {}", PortStr, e.what());
		}
	}
	ConfigPath = Path;
}

bool WDaemonConfig::Write()
{
	mINI::INIFile const File(ConfigPath);
	mINI::INIStructure  Ini;

	Ini["network"].set({
		{ "interface", NetworkInterfaceName },
		{ "ingress_interface", IngressNetworkInterfaceName },
		{ "cgroup_path", CGroupPath },
	});

	Ini["daemon"].set({
		{ "user", DaemonUser },
		{ "group", DaemonGroup },
		{ "socket_path", DaemonSocketPath },
		{ "ip_proc_socket_path", IpLinkProcSocketPath },
		{ "websocket_auth_token", WebSocketAuthToken },
		{ "socket_permissions", std::to_string(DaemonSocketMode) },
		{ "ignored_connection_history_app_names", WStringFormat::JoinStrings(IgnoredConnectionHistoryApps, ';') },
		{ "ignored_connection_history_remote_ports", WStringFormat::JoinStrings(IgnoredConnectionHistoryPorts, ';') },
	});

	return File.write(Ini, true);
}

void WDaemonConfig::SetDefaults()
{
	auto const Ifaces = WNetworkInterface::List();
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
	spdlog::debug("bumping memlock rlimit");
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
