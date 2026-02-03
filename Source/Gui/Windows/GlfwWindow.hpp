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

	static void ContentScaleCallback(GLFWwindow* Window, float XScale, float YScale);

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

	float GetMainScale() const { return MainScale; }
	void SetMainScale(float Scale) { MainScale = Scale; }
	
	// Helper function to scale sizes for DPI-aware windows
	static ImVec2 ScaleSize(ImVec2 const& Size)
	{
		float Scale = GetInstance().GetMainScale();
		return ImVec2(Size.x * Scale, Size.y * Scale);
	}
	
	static float ScaleValue(float Value)
	{
		return Value * GetInstance().GetMainScale();
	}
};
