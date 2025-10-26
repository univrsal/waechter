//
// Created by usr on 12/10/2025.
//

#include <spdlog/spdlog.h>

#include "GlfwWindow.hpp"

int main()
{
	if (!WGlfwWindow::GetInstance().Init())
	{
		return -1;
	}

	WGlfwWindow::GetInstance().RunLoop();

	spdlog::info("Shutting down...");
	return 0;
}