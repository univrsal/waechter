//
// Created by usr on 26/10/2025.
//

#include "ProcessMap.hpp"
#include "SocketInfo.hpp"
#include "spdlog/spdlog.h"

#include <signal.h>

bool IsProcessRunningByKill(pid_t pid)
{
	if (pid <= 0)
		return false;

	if (kill(pid, 0) == 0)
		return true; // process exists and we have permission

	if (errno == EPERM)
		return true; // process exists but we don't have permission

	// errno == ESRCH (no such process) or other errors -> treat as not running
	return false;
}

bool IsProcessRunningByProc(pid_t pid)
{
	if (pid <= 0)
		return false;

	std::string procPath = "/proc/" + std::to_string(pid);
	return (access(procPath.c_str(), F_OK) == 0);
}

// Convenience wrapper: try kill(0) first, fall back to /proc check
bool IsProcessRunning(pid_t pid)
{
	if (IsProcessRunningByKill(pid))
		return true;
	return IsProcessRunningByProc(pid);
}

void WProcessMap::RefreshAllTrafficCounters()
{
	TrafficCounter.Refresh();

	for (auto& [Cookie, SocketInfo] : Sockets)
	{
		SocketInfo->TrafficCounter.Refresh();
	}

	// Check if the process with this PID still exists
	if (TrafficCounter.GetState() != CS_PendingRemoval)
	{
		if (!IsProcessRunning(PID))
		{
			spdlog::info("Process {} has exited, marking for removal", PID);
			TrafficCounter.MarkForRemoval();
		}
	}
}

std::shared_ptr<WSocketInfo> WProcessMap::FindOrMapSocket(WSocketCookie Cookie)
{
	auto It = Sockets.find(Cookie);
	if (It != Sockets.end())
	{
		return It->second;
	}

	auto SocketInfo = std::make_shared<WSocketInfo>();
	SocketInfo->ParentProcess = this;
	Sockets.emplace(Cookie, SocketInfo);
	return SocketInfo;
}

void WProcessMap::ToJson(WJson::object& Json)
{
	Json[JSON_KEY_PID] = PID;
	Json[JSON_KEY_CMDLINE] = CmdLine;

	WJson::array SocketsArray;

	for (auto& [Cookie, SocketInfo] : Sockets)
	{
		WJson::object SocketJson;
		SocketInfo->ToJson(SocketJson);
		SocketJson[JSON_KEY_SOCKET_COOKIE] = static_cast<double>(Cookie);
		SocketsArray.emplace_back(SocketJson);
	}

	Json[JSON_KEY_SOCKETS] = SocketsArray;
}