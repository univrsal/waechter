//
// Created by usr on 09/10/2025.
//

#include <thread>

#include <spdlog/spdlog.h>
#include <bpf/libbpf.h>
#include <unistd.h>

#include "SignalHandler.hpp"
#include "DaemonConfig.hpp"
#include "WaechterEbpf.hpp"

int Run()
{
	WSignalHandler& SignalHandler = WSignalHandler::GetInstance();

	spdlog::info("Waechter daemon starting");

	libbpf_set_strict_mode(LIBBPF_STRICT_ALL);

	WDaemonConfig::GetInstance().LogConfig();

	WWaechterEbpf EbpfObj;

	if (auto Result = EbpfObj.Init() != EEbpfInitResult::SUCCESS)
	{
		spdlog::error("EbpfObj.Init() failed: {}", Result);
		return -1;
	}

	while (!SignalHandler.bStop)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
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