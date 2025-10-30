//
// Created by usr on 09/10/2025.
//

#pragma once

#include "Types.hpp"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <signal.h>

namespace stdfs = std::filesystem;

class WFilesystem
{
public:
	static bool Exists(const stdfs::path& p)
	{
		return stdfs::exists(p);
	}

	static bool Writable(const stdfs::path& p)
	{
		stdfs::perms pms = stdfs::status(p).permissions();
		return ((pms & stdfs::perms::owner_write) != stdfs::perms::none) || ((pms & stdfs::perms::group_write) != stdfs::perms::none) || ((pms & stdfs::perms::others_write) != stdfs::perms::none);
	}

	static std::string ReadProc(std::string const& Path)
	{
		std::ifstream FileStream(Path, std::ios::in | std::ios::binary);
		if (!FileStream)
			return {};

		std::ostringstream ss;
		ss << FileStream.rdbuf();
		std::string Content = ss.str();

		if (Content.empty())
			return "";

		// replace NUL separators with spaces
		std::ranges::replace(Content, '\0', ' ');

		// remove a trailing space that comes from the final NUL (if any)
		if (!Content.empty() && Content.back() == ' ')
			Content.pop_back();

		return Content;
	}

	static bool IsProcessRunningByKill(WProcessId PID)
	{
		if (PID <= 0)
			return false;

		if (kill(PID, 0) == 0)
			return true; // process exists and we have permission

		if (errno == EPERM)
			return true; // process exists but we don't have permission

		// errno == ESRCH (no such process) or other errors -> treat as not running
		return false;
	}

	static bool IsProcessRunningByProc(WProcessId PID)
	{
		if (PID <= 0)
			return false;

		std::string procPath = "/proc/" + std::to_string(PID);
		return (access(procPath.c_str(), F_OK) == 0);
	}

	static bool IsProcessRunning(WProcessId PID)
	{
		if (IsProcessRunningByKill(PID))
			return true;
		return IsProcessRunningByProc(PID);
	}
};