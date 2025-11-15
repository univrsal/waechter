//
// Created by usr on 02/11/2025.
//

#pragma once
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <string>
#include <vector>

class WIconResolver
{
	std::unordered_map<std::string, std::string> IconMap{};

	std::string              GetIconFromBinaryName(std::string const& Binary);
	std::vector<std::string> DesktopDirs = {
		"/usr/share/applications",
	};

public:
	std::string const& ResolveIcon(std::string const& Binary)
	{
		if (auto const Iter = IconMap.find(Binary); Iter != IconMap.end())
		{
			return Iter->second;
		}

		IconMap[Binary] = GetIconFromBinaryName(Binary);
		return IconMap[Binary];
	}
};
