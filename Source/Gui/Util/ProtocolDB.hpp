/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <unordered_map>
#include <string>

#include "Singleton.hpp"
#include "IPAddress.hpp"

extern std::unordered_map<uint16_t, std::string> const GTCPServices;
extern std::unordered_map<uint16_t, std::string> const GUDPServices;

class WProtocolDB final : public TSingleton<WProtocolDB>
{
	std::unordered_map<uint16_t, std::string> TCPPortServiceMap;
	std::unordered_map<uint16_t, std::string> UDPPortServiceMap;

public:
	void Init();

	[[nodiscard]] std::string const& GetServiceName(EProtocol::Type Protocol, uint16_t Port) const
	{
		static std::string const UnknownService = "unknown";
		std::unordered_map<uint16_t, std::string> const* TCPMap{};
		std::unordered_map<uint16_t, std::string> const* UDPMap{};

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
		TCPMap = &TCPPortServiceMap;
		UDPMap = &UDPPortServiceMap;
#else
		TCPMap = &GTCPServices;
		UDPMap = &GUDPServices;
#endif

		if (Protocol == EProtocol::TCP)
		{
			if (auto const It = TCPMap->find(Port); It != TCPMap->end())
			{
				return It->second;
			}
		}
		else if (Protocol == EProtocol::UDP)
		{
			if (auto const It = UDPMap->find(Port); It != UDPMap->end())
			{
				return It->second;
			}
		}
		return UnknownService;
	}
};
