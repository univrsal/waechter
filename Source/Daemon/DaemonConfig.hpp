//
// Created by usr on 09/10/2025.
//

#pragma once

#include <string>

#include "Singleton.hpp"

struct WDaemonConfig : public TSingleton<WDaemonConfig>
{
	std::string NetworkInterfaceName{};
	std::string CGroupPath{"/sys/fs/cgroup"};

	WDaemonConfig();

	void LogConfig();
private:
	void Load(std::string const& Path);
	void SetDefaults();
	void BumpMemlockRlimit();
};
