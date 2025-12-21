/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
// ReSharper disable once CppUnusedIncludeDirective
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <memory>
#include <string>

#include "Singleton.hpp"
#include "MainWindow.hpp"

class WGlfwWindow : public TSingleton<WGlfwWindow>
{
	GLFWwindow* Window{};
	float       MainScale{ 1.0f };

	std::unique_ptr<WMainWindow> MainWindow{};

	// Keep the ini filename alive for ImGui (ImGui stores a pointer to it)
	std::string ImGuiIniPath{};

public:
	bool Init();

	void RunLoop();

	~WGlfwWindow() override = default;

	WMainWindow* GetMainWindow() { return MainWindow.get(); }

	void Destroy();

	void SetTitle(std::string const& Title);
};
