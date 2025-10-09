//
// Created by usr on 09/10/2025.
//

#include "DaemonConfig.hpp"

#include "Filesystem.hpp"
#include "NetworkInterface.hpp"

#include <INIReader.h>
#include <spdlog/spdlog.h>

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
}

void WDaemonConfig::LogConfig()
{
	spdlog::info("network interface={}", NetworkInterfaceName);
}

void WDaemonConfig::Load(std::string const& Path)
{
	INIReader Reader(Path);

	if (Reader.ParseError() < 0)
	{
		spdlog::error("can't load '{}': {}", Path, Reader.ParseErrorMessage());
		return;
	}

	if (Reader.HasSection("network"))
	{
		if (Reader.HasValue("network", "interface"))
		{
			NetworkInterfaceName = Reader.Get("network", "interface", NetworkInterfaceName);
		}
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