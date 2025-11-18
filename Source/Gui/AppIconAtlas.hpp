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
#include "Icons/IconAtlas.hpp"

class WAppIconAtlas : public TSingleton<WAppIconAtlas>
{
	std::mutex                                Mutex;
	GLuint                                    TextureId{ 0 };
	std::unordered_map<std::string, WAtlasUv> UvData;
	std::optional<WAppIconAtlasData>          PendingData; // set by background thread, consumed by render thread

public:
	ImVec2 NoIconUv1{ 0.0f, 0.0f }, NoIconUv2{ 1.0f, 1.0f };
	ImVec2 ProcessIconUv1{ 0.0f, 0.0f }, ProcessIconUv2{ 1.0f, 1.0f };
	ImVec2 SystemIconUv1{ 0.0f, 0.0f }, SystemIconUv2{ 1.0f, 1.0f };

	WAppIconAtlas() = default;
	~WAppIconAtlas() = default;

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

	void DrawProcessIcon(ImVec2 Size) const;
	void DrawIconForApplication(std::string const& BinaryName, ImVec2 Size);
	void FromAtlasData(WBuffer& Data);
	// Call this on the render thread (with a current GL context)
	void UploadPendingIfAny();
	void DrawSystemIcon(ImVec2 Size) const
	{
		ImGui::Image(WIconAtlas::GetInstance().IconAtlasTextureId, Size, SystemIconUv1, SystemIconUv2);
	}
};
