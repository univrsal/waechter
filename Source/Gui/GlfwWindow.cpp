//
// Created by usr on 26/10/2025.
//

#include "GlfwWindow.hpp"

#include <spdlog/spdlog.h>

bool WGlfwWindow::Init()
{
	if (!glfwInit())
	{
		spdlog::critical("GLFW initialization failed!");
		return false;
	}

	Window = glfwCreateWindow(640, 480, "Wächter", nullptr, nullptr);
	if (!Window)
	{
		spdlog::critical("GLFW window creation failed!");
		glfwTerminate();
		return false;
	}
	glfwMakeContextCurrent(Window);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		glfwDestroyWindow(Window);
		glfwTerminate();
		spdlog::critical("Failed to initialize GLAD!");
		return false;
	}
	return true;
}

void WGlfwWindow::RunLoop()
{
	while (!glfwWindowShouldClose(Window))
	{
		glClear(GL_COLOR_BUFFER_BIT);
		glfwSwapBuffers(Window);
		glfwPollEvents();
	}
}

WGlfwWindow::~WGlfwWindow()
{
	glfwDestroyWindow(Window);
	glfwTerminate();
}
