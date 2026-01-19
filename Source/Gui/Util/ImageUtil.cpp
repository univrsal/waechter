/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ImageUtil.hpp"

#include <stb_image.h>
#include <spdlog/spdlog.h>

WTexture WImageUtils::LoadImageFromMemoryRGBA8(unsigned char const* ImageData, unsigned int ImageDataSize)
{
	WTexture Tex{};
	glGenTextures(1, &Tex.TextureId);
	glBindTexture(GL_TEXTURE_2D, Tex.TextureId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	auto ImageDataPtr =
		stbi_load_from_memory(ImageData, static_cast<int>(ImageDataSize), &Tex.Width, &Tex.Height, &Tex.NumChannels, 4);
	if (ImageDataPtr == nullptr)
	{
		spdlog::error("Failed to load icon atlas image from memory");
		return {};
	}

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Tex.Width, Tex.Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, ImageDataPtr);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);
	stbi_image_free(ImageDataPtr);
	return Tex;
}