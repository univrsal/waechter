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

int Run()
{
	WSignalHandler& SignalHandler = WSignalHandler::GetInstance();


	libbpf_set_strict_mode(LIBBPF_STRICT_ALL);

	WDaemonConfig::GetInstance().LogConfig();

	WWaechterEbpf EbpfObj{};

	spdlog::info("Waechter daemon starting");

	if (auto Result = EbpfObj.Init() != EEbpfInitResult::SUCCESS)
	{
		spdlog::error("EbpfObj.Init() failed: {}", Result);
		return -1;
	}

	if (!WDaemonConfig::GetInstance().DropPrivileges())
	{
		spdlog::error("Failed to drop privileges");
		return -1;
	}

	spdlog::info("Ebpf programs loaded and attached");

	// Poll ring buffers and periodically print aggregate stats
	auto LastPrint = std::chrono::steady_clock::now();

	while (!SignalHandler.bStop)
	{
		EbpfObj.PollRingBuffers(200);
		auto now = std::chrono::steady_clock::now();
		if (now - LastPrint >= std::chrono::seconds(1))
		{
			EbpfObj.PrintStats();
			LastPrint = now;
		}
	}

	return 0;
}

int main()
{
	if (geteuid() != 0)
	{
		spdlog::critical("Waechter daemon requires root");
		// return -1;
	}

	auto Ret = Run();
	spdlog::info("Waechter daemon stopped");
	return Ret;
}