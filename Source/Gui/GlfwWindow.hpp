//
// Created by usr on 26/10/2025.
//

#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Singleton.hpp"
#include "MainWindow.hpp"

class WGlfwWindow : public TSingleton<WGlfwWindow>
{
	GLFWwindow* Window{};
	float       MainScale{ 1.0f };

	WMainWindow MainWindow;

public:
	bool Init();

	void RunLoop();

	~WGlfwWindow();
};
