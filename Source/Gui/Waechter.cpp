/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "Windows/GlfwWindow.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32
// When building as a Windows GUI application, use WinMain as the entry point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	(void)hInstance;
	(void)hPrevInstance;
	(void)lpCmdLine;
	(void)nCmdShow;
#else
int main()
{
#endif

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