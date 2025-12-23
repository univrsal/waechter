/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "Daemon.hpp"
#include "DaemonConfig.hpp"

#include <spdlog/spdlog.h>
#include <unistd.h>

#include "EBPF/WaechterEbpf.hpp"
#include "Net/Resolver.hpp"
#include "Net/IPLink.hpp"

int main()
{
	if (std::getenv("INVOCATION_ID") != nullptr)
	{
		// Running under systemd so we don't need the timestamp from spdlog
		spdlog::set_pattern("[%^%l%$] %v");
	}

	if (geteuid() != 0)
	{
		spdlog::critical("Waechter daemon requires root");
		return -1;
	}

	libbpf_set_strict_mode(LIBBPF_STRICT_ALL);
	spdlog::info("Waechter daemon starting");
	WDaemonConfig::GetInstance().LogConfig();
	if (!WIPLink::GetInstance().Init())
	{
		spdlog::error("Failed to initialize IP link");
		WIPLink::GetInstance().Deinit();
		return -1;
	}

	if (!WDaemon::GetInstance().InitEbpfObj() || !WDaemon::GetInstance().InitSocket())
	{
		WIPLink::GetInstance().Deinit();
		return -1;
	}
	WResolver::GetInstance().Start();

	if (!WDaemonConfig::GetInstance().DropPrivileges())
	{
		spdlog::error("Failed to drop privileges");
		return -1;
	}

	WDaemon::RegisterSignalHandlers();

	spdlog::info("Ebpf programs loaded and attached");
	WDaemon::GetInstance().RunLoop();
	spdlog::info("Waechter daemon stopped");
	WIPLink::GetInstance().Deinit();
	WResolver::GetInstance().Stop();
	return 0;
}