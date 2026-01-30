/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ProtocolDB.hpp"

#include <cstring>
#include <string>

#ifndef _WIN32
	#include <netdb.h>
	#include <netinet/in.h>
#endif

#if __EMSCRIPTEN__
	#include "Json.hpp"
	#include "Assets.hpp"
#endif

#include "spdlog/spdlog.h"

void WProtocolDB::Init()
{
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
	// Iterate the services database and populate TCP/UDP maps
	setservent(0); // do not keep the DB open
	servent const* ServEnt = nullptr;
	while ((ServEnt = getservent()) != nullptr)
	{
		if (ServEnt->s_name == nullptr || ServEnt->s_proto == nullptr)
		{
			continue;
		}

		// s->s_port is in network byte order; convert to host order
		uint16_t    Port = ntohs(static_cast<uint16_t>(ServEnt->s_port));
		std::string Name(ServEnt->s_name);

		if (std::strcmp(ServEnt->s_proto, "tcp") == 0)
		{
			TCPPortServiceMap[Port] = Name;
		}
		else if (std::strcmp(ServEnt->s_proto, "udp") == 0)
		{
			UDPPortServiceMap[Port] = Name;
		}
	}

	endservent();
#else
	const auto ProtocolJson =
		std::string(reinterpret_cast<const char*>(GBuiltinProtocolDBData), GBuiltinProtocolDBSize);
	std::string Err;
	auto const  Json = WJson::parse(ProtocolJson, Err);
	if (Err.empty())
	{
		for (auto const& [Key, Name] : Json["tcp"].object_items())
		{
			uint16_t PortValue{};
			try
			{
				PortValue = static_cast<uint16_t>(std::stoul(Key));
			}
			catch (std::exception const&)
			{
				spdlog::warn("Invalid port number in protocol db: {}", Key);
				continue;
			}
			TCPPortServiceMap[PortValue] = Name.string_value();
		}
		for (auto const& [Key, Name] : Json["udp"].object_items())
		{
			uint16_t PortValue{};
			try
			{
				PortValue = static_cast<uint16_t>(std::stoul(Key));
			}
			catch (std::exception const&)
			{
				spdlog::warn("Invalid port number in protocol db: {}", Key);
				continue;
			}
			UDPPortServiceMap[PortValue] = Name.string_value();
		}
	}
	else
	{
		spdlog::error("Failed to parse builtin protocol db: {}", Err);
	}

#endif
	spdlog::info(
		"Protocol db initialized with {} TCP and {} UDP services", TCPPortServiceMap.size(), UDPPortServiceMap.size());
}
