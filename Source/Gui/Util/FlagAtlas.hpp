/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <string>

#ifdef __EMSCRIPTEN__
	#include <GLES3/gl3.h>
#else
	#include "glad/glad.h"
#endif
#include "imgui.h"

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
