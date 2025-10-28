//
// Created by usr on 26/10/2025.
//

#pragma once

#include <string>
#include <unordered_map>
#include <memory>

#include "TrafficCounter.hpp"
#include "Types.hpp"
#include "Json.hpp"

class WProcessMap;

class WApplicationMap
{
	std::string BinaryPath; // i.e. /usr/bin/myapp
	std::string BinaryName; // i.e. myapp

	std::unordered_map<WProcessId, std::shared_ptr<WProcessMap>> ChildProcesses;

public:
	WTrafficCounter TrafficCounter{};

	WApplicationMap(std::string BinaryPath_, std::string BinaryName_)
		: BinaryPath(std::move(BinaryPath_))
		, BinaryName(std::move(BinaryName_))
	{
	}

	void RefreshAllTrafficCounters();

	std::shared_ptr<WProcessMap> FindOrMapChildProcess(WProcessId PID, std::string const& CmdLine);

	std::unordered_map<WProcessId, std::shared_ptr<WProcessMap>>& GetChildProcesses()
	{
		return ChildProcesses;
	}

	void ToJson(WJson::object& Json);
};
