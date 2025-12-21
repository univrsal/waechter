/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <vector>
#include <unordered_map>
#include <string>

struct WAtlasUv
{
	float U1;
	float V1;
	float U2;
	float V2;

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(U1, V1, U2, V2);
	}
};

struct WAppIconAtlasData
{
	std::unordered_map<std::string, WAtlasUv> UvData;
	std::vector<unsigned char>                AtlasImageData;
	int                                       AtlasSize{ 64 };
	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(UvData, AtlasImageData, AtlasSize);
	}
};