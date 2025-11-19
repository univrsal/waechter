//
// Created by usr on 09/10/2025.
//

#include "Daemon.hpp"

#include <spdlog/spdlog.h>
#include <unistd.h>

#include "SignalHandler.hpp"
#include "DaemonConfig.hpp"
#include "WaechterEbpf.hpp"

int main()
{
	if (geteuid() != 0)
	{
		spdlog::critical("Waechter daemon requires root");
		return -1;
	}

	libbpf_set_strict_mode(LIBBPF_STRICT_ALL);
	spdlog::info("Waechter daemon starting");

	if (!WDaemon::GetInstance().InitEbpfObj() || !WDaemon::GetInstance().InitEbpfObj()
		|| !WDaemon::GetInstance().InitSocket())
	{
		return -1;
	}

	if (!WDaemonConfig::GetInstance().DropPrivileges())
	{
		spdlog::error("Failed to drop privileges");
		return -1;
	}

	spdlog::info("Ebpf programs loaded and attached");
	WDaemon::GetInstance().RunLoop();
	spdlog::info("Waechter daemon stopped");
	return 0;
}