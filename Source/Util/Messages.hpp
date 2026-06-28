/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <sstream>

// ReSharper disable CppUnusedIncludeDirective
#include "cereal/archives/binary.hpp"
#include "cereal/types/array.hpp"
// ReSharper restore CppUnusedIncludeDirective

#include "Buffer.hpp"

enum EMessageType : int8_t
{
	MT_Invalid = -1,
	MT_TrafficTree,
	MT_TrafficTreeUpdate,
	MT_AppIconAtlasData,
	MT_RuleUpdate,
	MT_Handshake,
	MT_ConnectionHistory,
	MT_ConnectionHistoryUpdate,
	MT_MemoryStats,
	MT_ResolveRequest,
	MT_ResolveResponse,
	MT_StatsRequest,
	MT_StatsResponse,
	MT_HistoryRequest,
	MT_HistoryResponse,

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

// Serializes a typed message into a framed string:
//   [1 byte: EMessageType] [cereal binary payload]
template <class T>
static std::string SerializeMessage(EMessageType Type, T const& Data)
{
	std::stringstream Os{};
	Os << Type;
	cereal::BinaryOutputArchive Archive(Os);
	Archive(Data);
	return Os.str();
}

// Deserializes the payload of a received buffer into OutData.
// Expects the buffer to contain [1 byte: EMessageType] followed by the cereal binary payload.
template <class T>
static bool DeserializeMessage(WBuffer const& Buffer, T& OutData)
{
	std::stringstream Ss;
	auto const Data = Buffer.GetWrittenChars();
	Ss.write(Data.data(), static_cast<std::streamsize>(Data.size()));
	Ss.seekg(1); // Skip message type byte
	try
	{
		cereal::BinaryInputArchive Iar(Ss);
		Iar(OutData);
	}
	catch (std::exception const&)
	{
		return false;
	}
	return true;
}
