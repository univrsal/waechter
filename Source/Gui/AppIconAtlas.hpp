/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <imgui.h>
#include <glad/glad.h>
#include <mutex>
#include <string>
#include <unordered_map>
#include <optional>

#include "Buffer.hpp"
#include "Singleton.hpp"
#include "Data/AppIconAtlasData.hpp"
#include "Icons/IconAtlas.hpp"

class WAppIconAtlas : public TSingleton<WAppIconAtlas>
{
	std::mutex                                Mutex;
	GLuint                                    TextureId{ 0 };
	std::unordered_map<std::string, WAtlasUv> UvData;
	std::optional<WAppIconAtlasData>          PendingData; // set by background thread, consumed by render thread

public:
	WAppIconAtlas() = default;
	~WAppIconAtlas() override = default;

	void Init();

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
