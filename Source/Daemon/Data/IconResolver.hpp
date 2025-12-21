/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <string>
#include <vector>

class WIconResolver
{
	std::unordered_map<std::string, std::string> IconMap{};

	std::string              GetIconFromBinaryName(std::string const& Binary) const;
	std::vector<std::string> DesktopDirs = {
		"/usr/share/applications",
	};

public:
	std::string const& ResolveIcon(std::string const& Binary, bool* bCached = nullptr)
	{
		if (auto const Iter = IconMap.find(Binary); Iter != IconMap.end())
		{
			if (bCached)
			{
				*bCached = true;
			}
			return Iter->second;
		}

		if (bCached)
		{
			*bCached = false;
		}

		IconMap[Binary] = GetIconFromBinaryName(Binary);
		return IconMap[Binary];
	}
};
