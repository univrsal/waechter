//
// Created by usr on 02/11/2025.
//

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

class WAppIconAtlas : public TSingleton<WAppIconAtlas>
{
	std::mutex                                Mutex;
	GLuint                                    TextureId{ 0 }, NoIconTextureId{ 0 }, ProcessIconTextureId{ 0 };
	std::unordered_map<std::string, WAtlasUv> UvData;
	std::optional<WAppIconAtlasData>          PendingData; // set by background thread, consumed by render thread

public:
	WAppIconAtlas() = default;
	~WAppIconAtlas() = default;

	std::mutex& GetMutex() { return Mutex; }

	void Init();

	void Cleanup()
	{
		if (TextureId != 0)
		{
			glDeleteTextures(1, &TextureId);
			TextureId = 0;
		}

		if (NoIconTextureId != 0)
		{
			glDeleteTextures(1, &NoIconTextureId);
			NoIconTextureId = 0;
		}

		if (ProcessIconTextureId != 0)
		{
			glDeleteTextures(1, &ProcessIconTextureId);
			ProcessIconTextureId = 0;
		}
	}

	void DrawProcessIcon(ImVec2 Size);
	void DrawIconForApplication(std::string const& BinaryName, ImVec2 Size);
	void FromAtlasData(WBuffer& Data);
	// Call this on the render thread (with a current GL context)
	void UploadPendingIfAny();
};
