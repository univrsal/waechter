//
// Created by usr on 18/11/2025.
//

#include "IconAtlas.hpp"

#define INCBIN_PREFIX G
#include <incbin.h>
#include <stb_image.h>
#include <spdlog/spdlog.h>

INCBIN(IconAtlas, ICON_ATLAS_IMAGE);
INCBIN(Logo, LOGO_IMAGE);

std::unordered_map<std::string, std::pair<ImVec2, ImVec2>> const ICON_ATLAS_UV = {
	{ "computer", std::make_pair(ImVec2(0.000000f, 0.000000f), ImVec2(0.250000f, 0.250000f)) },
	{ "noicon", std::make_pair(ImVec2(0.250000f, 0.000000f), ImVec2(0.500000f, 0.250000f)) },
	{ "phone", std::make_pair(ImVec2(0.500000f, 0.000000f), ImVec2(0.750000f, 0.250000f)) },
	{ "process", std::make_pair(ImVec2(0.750000f, 0.000000f), ImVec2(1.000000f, 0.250000f)) },
	{ "proxy", std::make_pair(ImVec2(0.000000f, 0.250000f), ImVec2(0.250000f, 0.500000f)) },
	{ "server", std::make_pair(ImVec2(0.250000f, 0.250000f), ImVec2(0.500000f, 0.500000f)) },
	{ "tor", std::make_pair(ImVec2(0.500000f, 0.250000f), ImVec2(0.750000f, 0.500000f)) },
	{ "vpn", std::make_pair(ImVec2(0.750000f, 0.250000f), ImVec2(1.000000f, 0.500000f)) },
};

void WIconAtlas::Load()
{
	glGenTextures(1, &IconAtlasTextureId);
	glBindTexture(GL_TEXTURE_2D, IconAtlasTextureId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	int            Width, Height, Channels;
	unsigned char* ImageDataPtr =
		stbi_load_from_memory(GIconAtlasData, static_cast<int>(GIconAtlasSize), &Width, &Height, &Channels, 4);
	if (ImageDataPtr == nullptr)
	{
		spdlog::error("Failed to load icon atlas image from memory");
		return;
	}

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, ImageDataPtr);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);
	stbi_image_free(ImageDataPtr);

	glGenTextures(1, &LogoTextureId);
	glBindTexture(GL_TEXTURE_2D, LogoTextureId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	ImageDataPtr =
		stbi_load_from_memory(GIconAtlasData, static_cast<int>(GIconAtlasSize), &Width, &Height, &Channels, 4);
	if (ImageDataPtr == nullptr)
	{
		spdlog::error("Failed to load icon atlas image from memory");
		return;
	}

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, ImageDataPtr);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);
	stbi_image_free(ImageDataPtr);
}

void WIconAtlas::Unload()
{
	if (IconAtlasTextureId != 0)
	{
		glDeleteTextures(1, &IconAtlasTextureId);
		IconAtlasTextureId = 0;
	}

	if (LogoTextureId != 0)
	{
		glDeleteTextures(1, &LogoTextureId);
		LogoTextureId = 0;
	}
}