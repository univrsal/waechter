/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "FlagAtlas.hpp"


#include "stb_image.h"
#include "spdlog/spdlog.h"

#include "CountryFlags.hpp"
#include "Assets.hpp"

void WFlagAtlas::DrawFlag(std::string const& CountryCode, ImVec2 Size)
{
	static ImVec2 const FlagSize{ 32.0f / 512.f, 24.0f / 512.f };
	static ImVec2 const PlaceholderUv1{ 480.0f / 512.f, 384.0f / 512.f };
	static ImVec2 const PlaceholderUv2{ 480.0f / 512.f + FlagSize.x, 384.0f / 512.f + FlagSize.y };

	if (TextureId == 0)
	{
		// Texture not loaded
		return;
	}

	if (FLAG_ATLAS_POS.contains(CountryCode))
	{
		auto  Pos = FLAG_ATLAS_POS.at(CountryCode);
		float U1 = static_cast<float>(Pos.first) / 512.0f;
		float V1 = static_cast<float>(Pos.second) / 512.0f;
		float U2 = U1 + FlagSize.x;
		float V2 = V1 + FlagSize.y;

		ImGui::Image(TextureId, Size, ImVec2(U1, V1), ImVec2(U2, V2));
	}
	else
	{
		// Draw empty placeholder
		ImGui::Image(TextureId, Size, PlaceholderUv1, PlaceholderUv2);
	}
}

void WFlagAtlas::Load()
{
	glGenTextures(1, &TextureId);
	glBindTexture(GL_TEXTURE_2D, TextureId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	int            Width, Height, Channels;
	unsigned char* ImageDataPtr =
		stbi_load_from_memory(GFlagAtlasData, static_cast<int>(GFlagAtlasSize), &Width, &Height, &Channels, 4);
	if (ImageDataPtr == nullptr)
	{
		spdlog::error("Failed to load flag atlas image from memory");
		return;
	}

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, ImageDataPtr);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);
	stbi_image_free(ImageDataPtr);
}

void WFlagAtlas::Unload()
{
	if (TextureId != 0)
	{
		glDeleteTextures(1, &TextureId);
		TextureId = 0;
	}
}