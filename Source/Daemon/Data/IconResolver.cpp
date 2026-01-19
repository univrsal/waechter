/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "IconResolver.hpp"

#include <fstream>
#include <vector>

#include "Filesystem.hpp"

inline std::string FindDesktopFile(std::string const& BinaryName, std::vector<std::string> const& DesktopDirs)
{
	auto BinaryNameLowercase = BinaryName;
	std::transform(BinaryNameLowercase.begin(), BinaryNameLowercase.end(), BinaryNameLowercase.begin(), ::tolower);

	for (auto const& Dir : DesktopDirs)
	{
		for (auto const& Entry : stdfs::recursive_directory_iterator(Dir))
		{
			if (Entry.path().extension() == ".desktop")
			{
				std::ifstream File(Entry.path());
				std::string   Line;
				while (std::getline(File, Line))
				{
					auto LineLowercase = Line;
					std::ranges::transform(LineLowercase, LineLowercase.begin(), ::tolower);
					if (LineLowercase.rfind("exec=", 0) == 0
						&& LineLowercase.find(BinaryNameLowercase) != std::string::npos)
					{
						spdlog::debug(
							"Found desktop file for binary '{}' at path '{}'", BinaryName, Entry.path().string());
						return Entry.path();
					}
				}
			}
		}
	}
	return "";
}

inline std::string GetIconName(std::string const& DesktopFilePath)
{
	std::ifstream File(DesktopFilePath);
	std::string   Line;
	while (std::getline(File, Line))
	{
		if (Line.rfind("Icon=", 0) == 0)
		{
			return Line.substr(5);
		}
	}
	return "";
}

inline std::string FindIconPathByName(std::string const& Name)
{
	// Iterate through all icon directories and look for {name}.png
	// If found, return the path to the icon
	for (auto const& Dir : stdfs::recursive_directory_iterator("/usr/share/icons/hicolor"))
	{
		if (Dir.path().extension() == ".png" && Dir.path().filename().string().find(Name) != std::string::npos)
		{
			return Dir.path();
		}
	}
	return "";
}

std::string WIconResolver::GetIconFromBinaryName(std::string const& Binary) const
{
	if (Binary.empty())
	{
		return "";
	}

	auto const DesktopFile = FindDesktopFile(Binary, DesktopDirs);
	if (DesktopFile.empty())
	{
		spdlog::debug("No desktop file found for binary '{}'", Binary);
		return "";
	}

	auto const Name = GetIconName(DesktopFile);
	if (Name.empty())
	{
		spdlog::debug("No icon name found in desktop file '{}'", DesktopFile);
		return "";
	}
	return FindIconPathByName(Name);
}