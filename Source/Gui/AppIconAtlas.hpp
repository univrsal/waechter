//
// Created by usr on 02/11/2025.
//

#pragma once
#include "Buffer.hpp"
#include "Singleton.hpp"

#include <imgui.h>
#include <glad/glad.h>
#include <mutex>
class WAppIconAtlasData;

class WAppIconAtlas : public TSingleton<WAppIconAtlas>
{
	std::mutex Mutex;
	GLuint     TextureId{};

public:
	WAppIconAtlas() = default;
	~WAppIconAtlas()
	{
		if (TextureId != 0)
		{
			glDeleteTextures(1, &TextureId);
		}
	}

	std::mutex& GetMutex()
	{
		return Mutex;
	}

	GLuint GetTextureId() const
	{
		return TextureId;
	}

	void FromAtlasData(WBuffer& Data);
};
