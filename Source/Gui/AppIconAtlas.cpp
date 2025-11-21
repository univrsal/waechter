//
// Created by usr on 02/11/2025.
//

#include "AppIconAtlas.hpp"

#include <cereal/archives/binary.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <sstream>
#include <spdlog/spdlog.h>

#include "Data/AppIconAtlasData.hpp"

void WAppIconAtlas::Init() {}

void WAppIconAtlas::DrawIconForApplication(std::string const& BinaryName, ImVec2 Size)
{
	if (!UvData.contains(BinaryName) || TextureId == 0)
	{
		// Draw no-icon placeholder
		WIconAtlas::GetInstance().DrawIcon("noicon", Size);
		return;
	}

	auto UV = UvData[BinaryName];

	ImGui::Image(TextureId, Size, ImVec2(UV.U1, UV.V1), ImVec2(UV.U2, UV.V2));
}

void WAppIconAtlas::FromAtlasData(WBuffer& Buffer)
{
	WAppIconAtlasData Data{};
	std::stringstream ss;
	ss.write(Buffer.GetData(), static_cast<long int>(Buffer.GetWritePos()));
	{
		ss.seekg(1); // Skip message type
		cereal::BinaryInputArchive iar(ss);
		iar(Data);
	}

	std::lock_guard Lock(Mutex);
	// Defer GL uploads to render thread to avoid using a context from network thread
	PendingData = std::move(Data);
}

void WAppIconAtlas::UploadPendingIfAny()
{
	std::optional<WAppIconAtlasData> Local;
	{
		std::lock_guard Lock(Mutex);
		if (!PendingData.has_value())
			return;
		Local.swap(PendingData);
	}

	// Now we are on the render thread with a current GL context
	auto& Data = *Local;

	// Update UV map
	{
		std::lock_guard Lock(Mutex);
		UvData = std::move(Data.UvData);
	}

	if (TextureId != 0)
	{
		glDeleteTextures(1, &TextureId);
		TextureId = 0;
	}

	glGenTextures(1, &TextureId);
	glBindTexture(GL_TEXTURE_2D, TextureId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Data.AtlasSize, Data.AtlasSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
		Data.AtlasImageData.data());

	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	spdlog::info("Uploaded app icon atlas texture: {}x{}, icons={}", Data.AtlasSize, Data.AtlasSize, UvData.size());
}