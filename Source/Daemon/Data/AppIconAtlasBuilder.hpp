//
// Created by usr on 02/11/2025.
//

#pragma once
#include <atomic>

#include "IconResolver.hpp"
#include "Data/AppIconAtlasData.hpp"
#include "Singleton.hpp"

class WAppIconAtlasBuilder : public TSingleton<WAppIconAtlasBuilder>
{
	WIconResolver     Resolver;
	std::atomic<bool> bDirty;

public:
	WIconResolver& GetResolver() { return Resolver; }
	bool           GetAtlasData(WAppIconAtlasData& outData, std::vector<std::string> const& BinaryNames,
				  std::size_t AtlasSize = 256, std::size_t IconSize = 32);

	void MarkDirty() { bDirty = true; }
	bool IsDirty() const { return bDirty; }
	void ClearDirty() { bDirty = false; }
};
