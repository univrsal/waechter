//
// Created by usr on 09/10/2025.
//

#include <thread>

#include <spdlog/spdlog.h>
#include <bpf/libbpf.h>

#include "SignalHandler.hpp"
#include "DaemonConfig.hpp"
#include "WaechterEpf.hpp"

int main()
{
	WSignalHandler& SignalHandler = WSignalHandler::GetInstance();

	spdlog::info("Waechter daemon starting");

	libbpf_set_strict_mode(LIBBPF_STRICT_ALL);

	WDaemonConfig::GetInstance().LogConfig();

	WWaechterEpf EbpfObj;

	if (auto Result = EbpfObj.Init() != EEbpfInitResult::SUCCESS)
	{
		spdlog::error("EbpfObj.Init() failed: {}", Result);
		return -1;
	}

	while (!SignalHandler.bStop)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	spdlog::info("Waechter daemon stopped");
	return 0;
}