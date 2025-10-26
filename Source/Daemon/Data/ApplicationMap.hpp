//
// Created by usr on 26/10/2025.
//

#pragma once

#include <string>
#include <unordered_map>
#include <memory>

#include "TrafficCounter.hpp"
#include "Types.hpp"

class WProcessMap;

class WApplicationMap
{
	std::string BinaryPath; // i.e. /usr/bin/myapp
	std::string BinaryName; // i.e. myapp

	std::unordered_map<WProcessId, std::shared_ptr<WProcessMap>> ChildProcesses;

public:
	WTrafficCounter TrafficCounter{};

	WApplicationMap(std::string BinaryPath, std::string BinaryName)
		: BinaryPath(std::move(BinaryPath))
		, BinaryName(std::move(BinaryName))
	{
	}

	std::shared_ptr<WProcessMap> FindOrMapChildProcess(WProcessId PID, std::string const& CmdLine);
};
