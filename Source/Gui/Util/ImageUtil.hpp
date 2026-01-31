/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#ifdef __EMSCRIPTEN__
	#include <GLES3/gl3.h>
#else
	#include "glad/glad.h"
#endif

struct WTexture
{
	GLuint TextureId;
	int    Width;
	int    Height;
	int    NumChannels;

	bool IsValid() const { return TextureId != 0; }
	void Unload()
	{
		if (TextureId != 0)
		{
			glDeleteTextures(1, &TextureId);
			TextureId = 0;
		}
	}
};

class WImageUtils
{
public:
	static WTexture LoadImageFromMemoryRGBA8(unsigned char const* ImageData, unsigned int ImageDataSize);
};