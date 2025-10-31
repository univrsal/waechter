#pragma once

#include "Buffer.hpp"

enum EMessageType : int8_t
{
	MT_Invalid = -1,
	MT_TrafficTree,
	MT_TrafficUpdate,
	MT_SetTcpLimit,
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
