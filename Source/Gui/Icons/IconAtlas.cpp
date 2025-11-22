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
	{ "computer", std::make_pair(ImVec2(0.003906f, 0.003906f), ImVec2(0.128906f, 0.128906f)) },
	{ "downloadallow", std::make_pair(ImVec2(0.136719f, 0.003906f), ImVec2(0.261719f, 0.128906f)) },
	{ "downloadblock", std::make_pair(ImVec2(0.269531f, 0.003906f), ImVec2(0.394531f, 0.128906f)) },
	{ "downloadlimit", std::make_pair(ImVec2(0.402344f, 0.003906f), ImVec2(0.527344f, 0.128906f)) },
	{ "noicon", std::make_pair(ImVec2(0.535156f, 0.003906f), ImVec2(0.660156f, 0.128906f)) },
	{ "phone", std::make_pair(ImVec2(0.667969f, 0.003906f), ImVec2(0.792969f, 0.128906f)) },
	{ "placeholder", std::make_pair(ImVec2(0.800781f, 0.003906f), ImVec2(0.925781f, 0.128906f)) },
	{ "process", std::make_pair(ImVec2(0.003906f, 0.136719f), ImVec2(0.128906f, 0.261719f)) },
	{ "proxy", std::make_pair(ImVec2(0.136719f, 0.136719f), ImVec2(0.261719f, 0.261719f)) },
	{ "server", std::make_pair(ImVec2(0.269531f, 0.136719f), ImVec2(0.394531f, 0.261719f)) },
	{ "tor", std::make_pair(ImVec2(0.402344f, 0.136719f), ImVec2(0.527344f, 0.261719f)) },
	{ "uploadallow", std::make_pair(ImVec2(0.535156f, 0.136719f), ImVec2(0.660156f, 0.261719f)) },
	{ "uploadblock", std::make_pair(ImVec2(0.667969f, 0.136719f), ImVec2(0.792969f, 0.261719f)) },
	{ "uploadlimit", std::make_pair(ImVec2(0.800781f, 0.136719f), ImVec2(0.925781f, 0.261719f)) },
	{ "vpn", std::make_pair(ImVec2(0.003906f, 0.269531f), ImVec2(0.128906f, 0.394531f)) },
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