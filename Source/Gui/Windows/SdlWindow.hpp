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
#include <SDL2/SDL.h>

#include "Singleton.hpp"
#include "MainWindow.hpp"

class WSdlWindow : public TSingleton<WSdlWindow>
{
	SDL_Window*   Window{};
	SDL_GLContext GlContext{};
	float         MainScale{ 1.0f };
	bool          bShouldClose{ false };

	std::unique_ptr<WMainWindow> MainWindow{};

	// Keep the ini filename alive for ImGui (ImGui stores a pointer to it)
	std::string ImGuiIniPath{};

	static char const* GetPreferredShaderVersion()
	{
#if __EMSCRIPTEN__
		return "#version 300 es";
#else
		return "#version 130";
#endif
	}

public:
	void Tick();
	bool Init();

	void RunLoop();

	~WSdlWindow() override = default;

	WMainWindow* GetMainWindow() { return MainWindow.get(); }

	SDL_Window* GetWindow() const { return Window; }

	void Destroy();

	void SetTitle(std::string const& Title) const;

	float GetMainScale() const { return MainScale; }
	void  SetMainScale(float Scale) { MainScale = Scale; }

	void RequestClose() { bShouldClose = true; }

	// Helper function to scale sizes for DPI-aware windows
	static ImVec2 ScaleSize(ImVec2 const& Size)
	{
		float Scale = GetInstance().GetMainScale();
		return ImVec2(Size.x * Scale, Size.y * Scale);
	}

	static float ScaleValue(float Value) { return Value * GetInstance().GetMainScale(); }
};
