//
// Created by usr on 04/11/2025.
//

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
	~WFlagAtlas()
	{
		Unload();
	}
};
