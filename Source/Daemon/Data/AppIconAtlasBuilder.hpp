/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <atomic>

#include "IconResolver.hpp"
#include "Singleton.hpp"
#include "Data/AppIconAtlasData.hpp"

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
