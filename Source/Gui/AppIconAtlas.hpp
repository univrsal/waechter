/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <mutex>
#include <string>
#include <unordered_map>
#include <optional>

#include "imgui.h"
#ifdef __EMSCRIPTEN__
	#include <GLES3/gl3.h>
#else
	#include "glad/glad.h"
#endif

#include "Buffer.hpp"
#include "Singleton.hpp"
#include "Data/AppIconAtlasData.hpp"

class WAppIconAtlas : public TSingleton<WAppIconAtlas>
{
	std::mutex                                Mutex;
	GLuint                                    TextureId{ 0 };
	std::unordered_map<std::string, WAtlasUv> UvData;
	std::optional<WAppIconAtlasData>          PendingData; // set by background thread, consumed by render thread

public:
	WAppIconAtlas() = default;
	~WAppIconAtlas() override = default;

	std::mutex& GetMutex() { return Mutex; }

	void Cleanup()
	{
		if (TextureId != 0)
		{
			glDeleteTextures(1, &TextureId);
			TextureId = 0;
		}
	}

	void DrawIconForApplication(std::string const& BinaryName, ImVec2 Size);
	void FromAtlasData(WBuffer& Data);
	// Call this on the render thread (with a current GL context)
	void UploadPendingIfAny();
};
