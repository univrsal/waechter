//
// Created by usr on 09/10/2025.
//

#pragma once

#include <filesystem>

namespace stdfs = std::filesystem;

class WFilesystem {
public:

	static bool Exists(const stdfs::path& p)
	{
		return stdfs::exists(p);
	}

	static bool Writable(const stdfs::path& p)
	{
		stdfs::perms pms = stdfs::status(p).permissions();
		return ((pms & stdfs::perms::owner_write) != stdfs::perms::none) ||
			   ((pms & stdfs::perms::group_write) != stdfs::perms::none) ||
			   ((pms & stdfs::perms::others_write) != stdfs::perms::none);
	}
};