/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <memory>
#include <string>

// ReSharper disable once CppUnusedIncludeDirective
#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include "Singleton.hpp"
#include "MainWindow.hpp"
#include "Util/TrayIcon.hpp"

class WGlfwWindow : public TSingleton<WGlfwWindow>
{
	GLFWwindow* Window{};
	float       MainScale{ 1.0f };

	std::unique_ptr<WMainWindow> MainWindow{};
	WTrayIcon                    TrayIcon{};
	// Keep the ini filename alive for ImGui (ImGui stores a pointer to it)
	std::string ImGuiIniPath{};

public:
	bool              bWindowHidden = false;
	std::atomic<bool> bShowRequested{};
	bool              Init();

	void RequestShow() { bShowRequested = true; }

	void RunLoop();
	WTrayIcon& GetTrayIcon() { return TrayIcon; }

	~WGlfwWindow() override = default;

	WMainWindow* GetMainWindow() { return MainWindow.get(); }

	void Destroy();

	void SetTitle(std::string const& Title) const;
};
