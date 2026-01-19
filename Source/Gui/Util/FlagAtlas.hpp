/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <glad/glad.h>
#include <imgui.h>
#include <string>

class WFlagAtlas
{
	GLuint TextureId{ 0 };
	void   Unload();

public:
	void DrawFlag(std::string const& CountryCode, ImVec2 Size);
	void Load();

	WFlagAtlas() = default;
	~WFlagAtlas() { Unload(); }
};
