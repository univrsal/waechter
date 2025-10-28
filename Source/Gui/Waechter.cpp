//
// Created by usr on 12/10/2025.
//

#include "Windows/GlfwWindow.hpp"

int main()
{
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

	if (!WGlfwWindow::GetInstance().Init())
	{
		return -1;
	}
	WGlfwWindow::GetInstance().RunLoop();
	return 0;
}