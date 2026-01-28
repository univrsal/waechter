/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "Windows/GlfwWindow.hpp"

int main()
{

#ifndef _WIN32
	// Just to suppress some warnings when opening URLs
	if (!getenv("LC_ALL") && !getenv("LANG"))
	{
		setenv("LANG", "C.UTF-8", 1);
	}
	if (!setlocale(LC_ALL, ""))
	{
		setenv("LC_ALL", "C.UTF-8", 1);
		setlocale(LC_ALL, "C.UTF-8");
	}
#endif
	if (!WGlfwWindow::GetInstance().Init())
	{
		return -1;
	}
	WGlfwWindow::GetInstance().RunLoop();
	WGlfwWindow::GetInstance().Destroy();
	return 0;
}