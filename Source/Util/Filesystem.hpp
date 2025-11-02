//
// Created by usr on 09/10/2025.
//

#pragma once

#include "ErrnoUtil.hpp"
#include "Types.hpp"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <pwd.h>
#include <signal.h>
#include <sys/stat.h>
#include <spdlog/spdlog.h>
#include <grp.h>
#include <unistd.h>
#include <cstdlib>
#include <sys/types.h>
#include <vector>

namespace stdfs = std::filesystem;

class WFilesystem
{
public:
	static bool Exists(stdfs::path const& p)
	{
		return stdfs::exists(p);
	}

	static bool Writable(stdfs::path const& p)
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

	// Read /proc file containing NUL-separated strings into a vector of strings.
	static std::vector<std::string> ReadProcNulSeparated(std::string const& Path)
	{
		std::ifstream FileStream(Path, std::ios::in | std::ios::binary);
		if (!FileStream)
			return {};

		std::string              buffer((std::istreambuf_iterator<char>(FileStream)), std::istreambuf_iterator<char>());
		std::vector<std::string> parts{};
		if (buffer.empty())
			return parts;

		std::string current{};
		for (char c : buffer)
		{
			if (c == '\0')
			{
				parts.emplace_back(std::move(current));
				current.clear();
			}
			else
			{
				current.push_back(c);
			}
		}
		// Some proc files may not end with a NUL; add remainder if any
		if (!current.empty())
		{
			parts.emplace_back(std::move(current));
		}

		// Drop a possible trailing empty element due to final NUL
		if (!parts.empty() && parts.back().empty())
		{
			parts.pop_back();
		}
		return parts;
	}

	// Readlink helper that returns the symlink target as string; returns empty on failure.
	static std::string ReadLink(std::string const& Path)
	{
		std::vector<char> buf(256);
		while (true)
		{
			ssize_t n = ::readlink(Path.c_str(), buf.data(), buf.size());
			if (n < 0)
			{
				return {};
			}
			if (static_cast<size_t>(n) < buf.size())
			{
				return std::string(buf.data(), static_cast<size_t>(n));
			}
			// Buffer too small, grow and retry
			buf.resize(buf.size() * 2);
		}
	}

	static std::string StripDeletedSuffix(std::string s)
	{
		constexpr char const* suffix = " (deleted)";
		size_t const          len = std::char_traits<char>::length(suffix);
		if (s.size() >= len && s.compare(s.size() - len, len, suffix) == 0)
		{
			s.erase(s.size() - len);
		}
		return s;
	}

	// Returns the absolute path of the executable for PID using /proc/[pid]/exe symlink if available.
	static std::string GetProcessExePath(WProcessId PID)
	{
		if (PID <= 0)
			return {};
		std::string link = "/proc/" + std::to_string(PID) + "/exe";
		std::string target = ReadLink(link);
		if (target.empty())
			return {};
		return StripDeletedSuffix(target);
	}

	// Returns the current working directory of the process (best-effort).
	static std::string GetProcessCwd(WProcessId PID)
	{
		if (PID <= 0)
			return {};
		std::string link = "/proc/" + std::to_string(PID) + "/cwd";
		std::string target = ReadLink(link);
		return StripDeletedSuffix(target);
	}

	// Returns argv vector for the given PID by reading /proc/[pid]/cmdline
	static std::vector<std::string> GetProcessCmdlineArgs(WProcessId PID)
	{
		if (PID <= 0)
			return {};
		return ReadProcNulSeparated("/proc/" + std::to_string(PID) + "/cmdline");
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

	static bool SetSocketOwnerAndPermsByName(std::string const& Path, std::string const& User, std::string const& Group, mode_t Mode)
	{
		uid_t Uid{};
		gid_t Gid{};

		if (passwd* Pw = getpwnam(User.c_str()))
		{
			Uid = Pw->pw_uid;
		}
		else
		{
			spdlog::error("user '{}' not found", User);
			return false;
		}

		if (group* Gr = getgrnam(Group.c_str()))
		{
			Gid = Gr->gr_gid;
		}
		else
		{
			spdlog::error("group '{}' not found", Group);
			return false;
		}

		if (chown(Path.c_str(), Uid, Gid) != 0)
		{
			spdlog::error("chown({}) failed: {} ({})", Path, WErrnoUtil::StrError(), errno);
			return false;
		}

		if (chmod(Path.c_str(), Mode) != 0)
		{
			spdlog::error("chmod({}) failed: {} ({})", Path, WErrnoUtil::StrError(), errno);
			return false;
		}

		return true;
	}
};
