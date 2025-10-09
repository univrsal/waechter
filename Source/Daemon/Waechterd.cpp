//
// Created by usr on 09/10/2025.
//

#include <thread>

#include <spdlog/spdlog.h>

#include "SignalHandler.hpp"

int main()
{
	WSignalHandler& SignalHandler = WSignalHandler::GetInstance();

	spdlog::info("Waechter daemon starting");

	while (!SignalHandler.bStop)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	spdlog::info("Waechter daemon stopped");
	return 0;
}