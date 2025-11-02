//
// Created by usr on 02/11/2025.
//

#include "IconResolver.hpp"

#include <fstream>
#include <cstdlib>
#include <vector>

#include "Filesystem.hpp"

inline std::string FindDesktopFile(std::string const& BinaryName, std::vector<std::string> const& DesktopDirs)
{

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
					if (Line.rfind("Exec=", 0) == 0 && Line.find(BinaryName) != std::string::npos)
					{
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
		if (Dir.path().extension() == ".png" && Dir.path().filename().string() == Name + ".png")
		{
			return Dir.path();
		}
	}
	return "";
}

std::string WIconResolver::GetIconFromBinaryName(std::string const& Binary)
{
	auto const DesktopFile = FindDesktopFile(Binary, DesktopDirs);
	if (DesktopFile.empty())
	{
		spdlog::warn("No desktop file found for binary '{}'", Binary);
		return "";
	}

	auto const Name = GetIconName(DesktopFile);
	if (Name.empty())
	{
		spdlog::warn("No icon name found in desktop file '{}'", DesktopFile);
		return "";
	}

	return FindIconPathByName(Name);
}

void WIconResolver::Init()
{
	// We have to delay this until the daemon has dropped to the unprivileged user
	DesktopDirs = {
		"/usr/share/applications",
	};
}