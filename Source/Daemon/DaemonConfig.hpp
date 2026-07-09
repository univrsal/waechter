/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <string>
#include <vector>
#include <cstdint>

#include "Singleton.hpp"

#include <algorithm>

struct WDaemonConfig final : TSingleton<WDaemonConfig>
{
	std::string NetworkInterfaceName{};
	std::string IngressNetworkInterfaceName{}; // same as NetworkInterfaceName if empty
	std::string CGroupPath{ "/sys/fs/cgroup" };
	std::string DaemonUser{ "nobody" };
	std::string DaemonGroup{ "nogroup" };
	std::string DaemonSocketPath{ "/var/run/waechterd.sock" };
	std::string IpLinkProcSocketPath{ "/var/run/waechter-iplink.sock" };
	std::string EbpfProgramObjectPath{ "./waechter-ebpf.o" };
	std::string WebSocketAuthToken{};
	std::vector<std::string> IgnoredConnectionHistoryApps{};
	std::vector<uint16_t>    IgnoredConnectionHistoryPorts{};
	bool                     bFirstTimeSetupRun{};

	mode_t      DaemonSocketMode{ 0660 };

	std::string ConfigPath{};

	WDaemonConfig();

	void LogConfig();

	bool DropPrivileges();

	static void BTFTest();

	bool IsIgnoredConnectionHistoryApp(std::string const& AppName)
	{
		return std::ranges::any_of(
			IgnoredConnectionHistoryApps, [&](auto const& IgnoredApp) { return AppName == IgnoredApp; });
	}

	bool IsIgnoredConnectionHistoryPort(uint16_t Port)
	{
		return std::ranges::any_of(
			IgnoredConnectionHistoryPorts, [&](auto const& IgnoredPort) { return Port == IgnoredPort; });
	}

	void Load(std::string const& Path);

private:
	bool Write();

	void SetDefaults();

	static void BumpMemlockRlimit();
};
