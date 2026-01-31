/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "Buffer.hpp"

enum EMessageType : int8_t
{
	MT_Invalid = -1,
	MT_TrafficTree,
	MT_ResolvedAddresses,
	MT_TrafficTreeUpdate,
	MT_AppIconAtlasData,
	MT_RuleUpdate,
	MT_Handshake,
	MT_ConnectionHistory,
	MT_ConnectionHistoryUpdate,
	MT_MemoryStats,

	MT_Count
};

#ifndef _WIN32
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

static EMessageType ReadMessageTypeFromBuffer(WBuffer& Buf)
{
	int8_t Type;
	if (!Buf.Read(Type))
	{
		return MT_Invalid;
	}

	if (Type < 0 || Type > static_cast<int8_t>(MT_Count))
	{
		return MT_Invalid;
	}

	return static_cast<EMessageType>(Type);
}
