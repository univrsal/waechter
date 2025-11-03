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

		std::string              Buffer((std::istreambuf_iterator(FileStream)), std::istreambuf_iterator<char>());
		std::vector<std::string> Parts{};
		if (Buffer.empty())
			return Parts;

		std::string Current{};
		for (char C : Buffer)
		{
			if (C == '\0')
			{
				Parts.emplace_back(std::move(Current));
				Current.clear();
			}
			else
			{
				Current.push_back(C);
			}
		}
		// Some proc files may not end with a NUL; add remainder if any
		if (!Current.empty())
		{
			Parts.emplace_back(std::move(Current));
		}

		// Drop a possible trailing empty element due to final NUL
		if (!Parts.empty() && Parts.back().empty())
		{
			Parts.pop_back();
		}
		return Parts;
	}

	// Readlink helper that returns the symlink target as string; returns empty on failure.
	static std::string ReadLink(std::string const& Path)
	{
		std::vector<char> Buf(256);
		while (true)
		{
			ssize_t N = ::readlink(Path.c_str(), Buf.data(), Buf.size());
			if (N < 0)
			{
				return {};
			}
			if (static_cast<size_t>(N) < Buf.size())
			{
				return { Buf.data(), static_cast<size_t>(N) };
			}
			// Buffer too small, grow and retry
			Buf.resize(Buf.size() * 2);
		}
	}

	static std::string StripDeletedSuffix(std::string S)
	{
		constexpr char const* Suffix = " (deleted)";
		size_t const          Len = std::char_traits<char>::length(Suffix);
		if (S.size() >= Len && S.compare(S.size() - Len, Len, Suffix) == 0)
		{
			S.erase(S.size() - Len);
		}
		return S;
	}

	// Returns the absolute path of the executable for PID using /proc/[pid]/exe symlink if available.
	static std::string GetProcessExePath(WProcessId PID)
	{
		if (PID <= 0)
			return {};
		std::string Link = "/proc/" + std::to_string(PID) + "/exe";
		std::string Target = ReadLink(Link);
		if (Target.empty())
			return {};
		return StripDeletedSuffix(Target);
	}

	// Returns the current working directory of the process (best-effort).
	static std::string GetProcessCwd(WProcessId PID)
	{
		if (PID <= 0)
			return {};
		std::string Link = "/proc/" + std::to_string(PID) + "/cwd";
		std::string Target = ReadLink(Link);
		return StripDeletedSuffix(Target);
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

		std::string ProcPath = "/proc/" + std::to_string(PID);
		return (access(ProcPath.c_str(), F_OK) == 0);
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
