//
// Created by usr on 09/10/2025.
//

#include <thread>

#include <spdlog/spdlog.h>
#include <unistd.h>

#include "SignalHandler.hpp"
#include "DaemonConfig.hpp"
#include "EbpfData.hpp"
#include "WaechterEbpf.hpp"
#include "Communication/DaemonSocket.hpp"

int Run()
{
	WSignalHandler& SignalHandler = WSignalHandler::GetInstance();

	libbpf_set_strict_mode(LIBBPF_STRICT_ALL);

	WDaemonConfig::GetInstance().LogConfig();
	WDaemonConfig::BTFTest();

	WWaechterEbpf EbpfObj{};
	WDaemonSocket DaemonSocket(WDaemonConfig::GetInstance().DaemonSocketPath);

	spdlog::info("Waechter daemon starting");

	auto Result = EbpfObj.Init();
	if (Result != EEbpfInitResult::Success)
	{
		spdlog::error("EbpfObj.Init() failed: {}", static_cast<int>(Result));
		return -1;
	}

	if (!WDaemonConfig::GetInstance().DropPrivileges())
	{
		spdlog::error("Failed to drop privileges");
		return -1;
	}
	DaemonSocket.StartListenThread();

	spdlog::info("Ebpf programs loaded and attached");

	// Poll ring buffers and periodically print aggregate stats
	auto LastPrint = std::chrono::steady_clock::now();

	while (!SignalHandler.bStop)
	{
		EbpfObj.UpdateData();
		auto now = std::chrono::steady_clock::now();
		if (now - LastPrint >= std::chrono::milliseconds(5000))
		{
			EbpfObj.PrintStats();
			LastPrint = now;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	DaemonSocket.Stop();

	return 0;
}

int main()
{
	if (geteuid() != 0)
	{
		spdlog::critical("Waechter daemon requires root");
		return -1;
	}

	auto Ret = Run();
	spdlog::info("Waechter daemon stopped");
	return Ret;
}