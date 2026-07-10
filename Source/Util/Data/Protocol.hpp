/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#define WAECHTER_PROTOCOL_VERSION 1
#include <cstdint>
#include <string>
#include <vector>

#include "Types.hpp"

namespace spdlog::level
{
	enum level_enum : int;
}
struct WProtocolHandshake
{
	uint8_t     ProtocolVersion{ WAECHTER_PROTOCOL_VERSION };
	WSec        SystemBootTime;
	std::string CommitHash{};

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(ProtocolVersion, CommitHash, SystemBootTime);
	}
};

struct WDaemonConfigMessage
{
	std::vector<std::string> NetworkInterfaces;
	bool                     bFirstTimeSetupRan{ true };
	std::string              SocketPath{};
	std::string              DaemonUser{};
	std::string              DaemonGroup{};
	std::string              MainInterface{};
	std::string              VpnInterface{};
	int                      SocketMode{};

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(NetworkInterfaces, bFirstTimeSetupRan, SocketPath, DaemonUser, DaemonGroup, MainInterface, VpnInterface,
			SocketMode);
	}
};

struct WDaemonLogMessage
{
	spdlog::level::level_enum Level{};
	std::string               Message{};

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(Level, Message);
	}
};