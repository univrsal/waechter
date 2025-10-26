//
// Created by usr on 12/10/2025.
//

#include "GlfwWindow.hpp"

int main()
{
	if (!WGlfwWindow::GetInstance().Init())
	{
		return -1;
	}
	WGlfwWindow::GetInstance().RunLoop();
	return 0;
}