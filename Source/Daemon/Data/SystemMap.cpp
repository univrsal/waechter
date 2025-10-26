//
// Created by usr on 26/10/2025.
//

#include "SystemMap.hpp"

#include <spdlog/spdlog.h>
#include <fstream>
#include <ranges>
#include <algorithm>

#include "ApplicationMap.hpp"
#include "ProcessMap.hpp"

static std::string ReadProc(std::string const& Path)
{
	std::ifstream FileStream(Path, std::ios::in | std::ios::binary);
	if (!FileStream)
		return {};

	// read entire file
	std::string Content((std::istreambuf_iterator<char>(FileStream)), std::istreambuf_iterator<char>());
	if (Content.empty())
	{
		return "";
	}

	// replace NUL separators with spaces
	std::ranges::replace(Content, '\0', ' ');

	// remove a trailing space that comes from the final NUL (if any)
	if (!Content.empty() && Content.back() == ' ')
		Content.pop_back();

	return Content;
}

std::shared_ptr<WSocketInfo> WSystemMap::MapSocket(WSocketCookie SocketCookie, WProcessId PID, bool bSilentFail)
{
	if (PID == 0)
	{
		if (!bSilentFail)
		{
			spdlog::warn("{}: Attempted to map socket cookie {} to PID 0", __FUNCTION__, SocketCookie);
		}
		return {};
	}

	// Read the command line from /proc/[pid]/cmdline
	std::string CmdLinePath = "/proc/" + std::to_string(PID) + "/cmdline";
	std::string CommPath = "/proc/" + std::to_string(PID) + "/comm";
	auto        CmdLine = ReadProc(CmdLinePath);
	auto        Comm = ReadProc(CommPath);

	// Remove trailing newline from Comm if present
	if (!Comm.empty() && Comm.back() == '\n')
	{
		Comm.pop_back();
	}

	auto App = FindOrMapApplication(Comm);
	auto Process = App->FindOrMapChildProcess(PID, CmdLine);
	return Process->FindOrMapSocket(SocketCookie);
}

std::shared_ptr<WApplicationMap> WSystemMap::FindOrMapApplication(std::string const& AppName)
{
	auto It = Applications.find(AppName);
	if (It != Applications.end())
	{
		return It->second;
	}
	spdlog::info("Mapped new application: {}", AppName);
	auto AppMap = std::make_shared<WApplicationMap>(AppName, AppName);
	Applications.emplace(AppName, AppMap);
	return AppMap;
}