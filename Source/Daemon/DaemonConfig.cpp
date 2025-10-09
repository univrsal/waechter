//
// Created by usr on 09/10/2025.
//

#include "DaemonConfig.hpp"

#include <sys/resource.h>
#include <INIReader.h>
#include <spdlog/spdlog.h>

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
