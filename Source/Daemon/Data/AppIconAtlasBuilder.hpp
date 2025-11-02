//
// Created by usr on 02/11/2025.
//

#pragma once
#include "IconResolver.hpp"

#include "Data/AppIconAtlasData.hpp"
#include "Singleton.hpp"

class WAppIconAtlasBuilder : public TSingleton<WAppIconAtlasBuilder>
{
	WIconResolver Resolver;

public:
	bool GetAtlasData(WAppIconAtlasData& outData, std::vector<std::string> const& BinaryNames, std::size_t AtlasSize = 256);

	void Init() { Resolver.Init(); }
};
