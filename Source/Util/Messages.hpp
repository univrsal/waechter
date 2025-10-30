#pragma once

#include <unistd.h>
#include <memory>
#include <utility>

#include "IPAddress.hpp"
#include "Buffer.hpp"
#include "Types.hpp"
#include "EBPFCommon.h"

#define WMESSAGE(clazz, type) \
	clazz() : WMessage(type) {}

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
