/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#define WAECHTER_PROTOCOL_VERSION 1
#include <cstdint>
#include <string>

struct WProtocolHandshake
{
	uint8_t     ProtocolVersion{ WAECHTER_PROTOCOL_VERSION };
	std::string CommitHash{};

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(ProtocolVersion, CommitHash);
	}
};