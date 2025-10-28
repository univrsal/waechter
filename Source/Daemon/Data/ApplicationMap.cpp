//
// Created by usr on 26/10/2025.
//

#include "ApplicationMap.hpp"

#include "ProcessMap.hpp"
#include "spdlog/spdlog.h"

#include <ranges>

void WApplicationMap::RefreshAllTrafficCounters()
{
	RefreshTrafficCounter();

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
	Json[JSON_KEY_UPLOAD] = GetTrafficCounter().GetUploadSpeed();
	Json[JSON_KEY_DOWNLOAD] = GetTrafficCounter().GetDownloadSpeed();
	Json[JSON_KEY_ID] = static_cast<double>(GetItemId());

	WJson::array ProcessArray;

	for (auto& ProcessInfo : ChildProcesses | std::views::values)
	{
		WJson::object ProcessJson;
		ProcessInfo->ToJson(ProcessJson);
		ProcessArray.emplace_back(ProcessJson);
	}

	Json[JSON_KEY_PROCESSES] = ProcessArray;
}