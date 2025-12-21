/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <unordered_map>
#include <string>
#include <cstdint>

#include "Singleton.hpp"
#include "IPAddress.hpp"

class WProtocolDB final : public TSingleton<WProtocolDB>
{
	std::unordered_map<uint16_t, std::string> TCPPortServiceMap;
	std::unordered_map<uint16_t, std::string> UDPPortServiceMap;

public:
	void Init();

	std::string const& GetServiceName(EProtocol::Type Protocol, uint16_t Port) const
	{
		static std::string const UnknownService = "unknown";

		if (Protocol == EProtocol::TCP)
		{
			if (auto It = TCPPortServiceMap.find(Port); It != TCPPortServiceMap.end())
			{
				return It->second;
			}
		}
		else if (Protocol == EProtocol::UDP)
		{
			if (auto It = UDPPortServiceMap.find(Port); It != UDPPortServiceMap.end())
			{
				return It->second;
			}
		}
		return UnknownService;
	}
};
