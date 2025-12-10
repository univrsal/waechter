//
// Created by usr on 09/10/2025.
//

#include "Daemon.hpp"
#include "DaemonConfig.hpp"

#include <spdlog/spdlog.h>
#include <unistd.h>

#include "SignalHandler.hpp"
#include "WaechterEbpf.hpp"
#include "Net/Resolver.hpp"
#include "Net/IPLink.hpp"
#include "Net/NetLink.hpp"

int main()
{
	if (geteuid() != 0)
	{
		spdlog::critical("Waechter daemon requires root");
		return -1;
	}

	if (!WNetLink::GetInstance().CreateIfbDevice("ifb0"))
	{
		spdlog::critical("Failed to create IB device");
		goto exit;
	}

	libbpf_set_strict_mode(LIBBPF_STRICT_ALL);
	spdlog::info("Waechter daemon starting");
	if (!WIPLink::GetInstance().Init())
	{
		spdlog::error("Failed to initialize IP link");
		goto exit;
	}

	if (!WDaemon::GetInstance().InitEbpfObj() || !WDaemon::GetInstance().InitSocket())
	{
		goto exit;
	}
	WResolver::GetInstance().Start();

	if (!WDaemonConfig::GetInstance().DropPrivileges())
	{
		spdlog::error("Failed to drop privileges");
		goto exit;
	}

	WDaemon::RegisterSignalHandlers();

	spdlog::info("Ebpf programs loaded and attached");
	WDaemon::GetInstance().RunLoop();
	spdlog::info("Waechter daemon stopped");

exit:
	WIPLink::GetInstance().Deinit();
	WResolver::GetInstance().Stop();
	WNetLink::GetInstance().DeleteIfbDevice("ifb0");
	return 0;
}