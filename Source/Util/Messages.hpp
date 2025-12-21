/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
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
	MT_Handshake
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

static EMessageType ReadMessageTypeFromBuffer(WBuffer& Buf)
{
	EMessageType Type;
	if (!Buf.Read(Type))
	{
		return MT_Invalid;
	}

	return Type;
}
