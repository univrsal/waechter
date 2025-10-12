//
// Created by usr on 09/10/2025.
//

#include <thread>

#include <spdlog/spdlog.h>
#include <bpf/libbpf.h>
#include <unistd.h>
#include <pwd.h>

#include "SignalHandler.hpp"
#include "DaemonConfig.hpp"
#include "EbpfData.hpp"
#include "WaechterEbpf.hpp"

int Run()
{
	WSignalHandler& SignalHandler = WSignalHandler::GetInstance();


	libbpf_set_strict_mode(LIBBPF_STRICT_ALL);

	WDaemonConfig::GetInstance().LogConfig();

	WWaechterEbpf* EbpfObj = new WWaechterEbpf;

	spdlog::info("Waechter daemon starting");

	if (auto Result = EbpfObj->Init() != EEbpfInitResult::SUCCESS)
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


	while (!SignalHandler.bStop)
	{
		EbpfObj->PrintStats();
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}

	delete EbpfObj;
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