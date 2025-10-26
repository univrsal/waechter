//
// Created by usr on 26/10/2025.
//

#include "ApplicationMap.hpp"

#include "ProcessMap.hpp"
#include "spdlog/spdlog.h"

std::shared_ptr<WProcessMap> WApplicationMap::FindOrMapChildProcess(WProcessId PID, std::string const& CmdLine)
{
	auto It = ChildProcesses.find(PID);

	if (It != ChildProcesses.end())
	{
		return It->second;
	}
	spdlog::info("Mapping new process {}", PID);
	auto ProcessInfo = std::make_shared<WProcessMap>(PID, CmdLine, this);
	ChildProcesses.emplace(PID, ProcessInfo);
	return ProcessInfo;
}