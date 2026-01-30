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
	spdlog::info(
		"Protocol db initialized with {} TCP and {} UDP services", TCPPortServiceMap.size(), UDPPortServiceMap.size());
#else
	spdlog::info(
		"Using builtin protocol db ({} tcp protocols, {} udp protocols)", GTCPServices.size(), GUDPServices.size());
#endif
}
