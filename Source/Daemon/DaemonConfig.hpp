//
// Created by usr on 09/10/2025.
//

#pragma once

#include <string>

#include "Singleton.hpp"

struct WDaemonConfig : public TSingleton<WDaemonConfig>
{
	std::string NetworkInterfaceName{};
	std::string CGroupPath{ "/sys/fs/cgroup" };
	std::string DaemonUser{ "nobody" };
	std::string DaemonGroup{ "nogroup" };
	std::string DaemonSocketPath{ "/var/run/waechterd.sock" };
	std::string EbpfProgramObjectPath{ "./waechter-ebpf.o" };
	mode_t      DaemonSocketMode{ 0660 };
	WDaemonConfig();

	void LogConfig();

	bool DropPrivileges();

	static void BTFTest();

private:
	void Load(std::string const& Path);
	void SetDefaults();

	static void BumpMemlockRlimit();
};
