//
// Created by usr on 09/10/2025.
//

#include "Daemon.hpp"
#include "DaemonConfig.hpp"

#include <spdlog/spdlog.h>
#include <unistd.h>

#include "WaechterEbpf.hpp"
#include "Net/Resolver.hpp"
#include "Net/IPLink.hpp"

int main()
{
	if (geteuid() != 0)
	{
		spdlog::critical("Waechter daemon requires root");
		return -1;
	}

	libbpf_set_strict_mode(LIBBPF_STRICT_ALL);
	spdlog::info("Waechter daemon starting");
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