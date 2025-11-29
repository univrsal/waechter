//
// Created by usr on 29/11/2025.
//

#pragma once
#include <glad/glad.h>

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