//
// Created by usr on 26/10/2025.
//

#include "ApplicationMap.hpp"

#include "ProcessMap.hpp"
#include "spdlog/spdlog.h"

void WApplicationMap::RefreshAllTrafficCounters()
{
	TrafficCounter.Refresh();

	for (auto& [PID, ProcessInfo] : ChildProcesses)
	{
		ProcessInfo->RefreshAllTrafficCounters();
	}
}

std::shared_ptr<WProcessMap> WApplicationMap::FindOrMapChildProcess(WProcessId PID, std::string const& CmdLine)
{
	auto It = ChildProcesses.find(PID);

	if (It != ChildProcesses.end())
	{
		return It->second;
	}
	spdlog::debug("Mapping new process {}", PID);
	auto ProcessInfo = std::make_shared<WProcessMap>(PID, CmdLine, this);
	ChildProcesses.emplace(PID, ProcessInfo);
	return ProcessInfo;
}

void WApplicationMap::ToJson(WJson::object& Json)
{
	Json[JSON_KEY_BINARY_PATH] = BinaryPath;
	Json[JSON_KEY_BINARY_NAME] = BinaryName;
	Json[JSON_KEY_UPLOAD] = TrafficCounter.GetUploadSpeed();
	Json[JSON_KEY_DOWNLOAD] = TrafficCounter.GetDownloadSpeed();

	WJson::array ProcessArray;

	for (auto& [PID, ProcessInfo] : ChildProcesses)
	{
		WJson::object ProcessJson;
		ProcessInfo->ToJson(ProcessJson);
		ProcessArray.emplace_back(ProcessJson);
	}

	Json[JSON_KEY_PROCESSES] = ProcessArray;
}