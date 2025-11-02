//
// Created by usr on 02/11/2025.
//

#include "AppIconAtlas.hpp"

#include <cereal/archives/binary.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <sstream>

#include "Data/AppIconAtlasData.hpp"

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
	if (TextureId != 0)
	{
		glDeleteTextures(1, &TextureId);
	}

	glGenTextures(1, &TextureId);
	glBindTexture(GL_TEXTURE_2D, TextureId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Data.AtlasSize, Data.AtlasSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, Data.AtlasImageData.data());

	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);
}