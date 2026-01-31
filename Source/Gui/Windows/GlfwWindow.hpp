/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <memory>
#include <string>

// ReSharper disable once CppUnusedIncludeDirective
#ifndef __EMSCRIPTEN__
	#include "glad/glad.h"
#endif
#include "GLFW/glfw3.h"

#include "Singleton.hpp"
#include "MainWindow.hpp"

class WGlfwWindow : public TSingleton<WGlfwWindow>
{
	GLFWwindow* Window{};
	float       MainScale{ 1.0f };

	std::unique_ptr<WMainWindow> MainWindow{};

	// Keep the ini filename alive for ImGui (ImGui stores a pointer to it)
	std::string ImGuiIniPath{};

	static char const* GetPreferredShaderVersion()
	{
#if EMSCRIPTEN
		return "#version 300 es";
#else
		return "#version 130";
#endif
	}

public:
	void Tick() const;
	bool Init();

	void RunLoop();

	~WGlfwWindow() override = default;

	WMainWindow* GetMainWindow() { return MainWindow.get(); }

	void Destroy();

	void SetTitle(std::string const& Title) const;
};
