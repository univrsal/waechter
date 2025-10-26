//
// Created by usr on 26/10/2025.
//

#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <utility>

#include "TrafficCounter.hpp"
#include "Types.hpp"

class WApplicationMap;
class WSocketInfo;

class WProcessMap
{
	WProcessId       PID{};
	std::string      CmdLine{}; // full command line with args
	WApplicationMap* ParentAppMap{};

	std::unordered_map<WSocketCookie, std::shared_ptr<WSocketInfo>> Sockets{};

public:
	WTrafficCounter TrafficCounter{};

	WProcessMap(WProcessId PID, std::string CmdLine, WApplicationMap* ParentAppMap)
		: PID(PID)
		, CmdLine(std::move(CmdLine))
		, ParentAppMap(ParentAppMap)
	{
	}

	WApplicationMap* GetParentApplicationMap() const
	{
		return ParentAppMap;
	}

	void RefreshAllTrafficCounters();

	std::shared_ptr<WSocketInfo> FindOrMapSocket(WSocketCookie Cookie);
};
