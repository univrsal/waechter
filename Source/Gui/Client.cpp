/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "Client.hpp"

#include <cstdint>
#include <spdlog/spdlog.h>

#include "AppIconAtlas.hpp"
#include "Messages.hpp"
#include "Data/Protocol.hpp"

inline bool ReadU32(char const* DataPtr, size_t N, uint32_t& outI32)
{
	if (N < 4)
		return false;
	uint32_t Val;
	std::memcpy(&Val, DataPtr, 4);
	outI32 = Val;
	return true;
}

void WClient::ConnectionThreadFunction()
{
	WBuffer Buf{};   // per-read buffer from socket
	WBuffer Accum{}; // accumulator for framing
	while (Running)
	{
		if (!EnsureConnected())
		{
			for (int i = 0; i < 20 && Running; ++i)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
		}
		Buf.Reset();
		bool bDataToRead{};
		if (!Socket->Receive(Buf, &bDataToRead))
		{
			Socket->Close();
			for (int i = 0; i < 20 && Running; ++i)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
			continue;
		}

		if (!bDataToRead)
		{
			for (int i = 0; i < 5 && Running; ++i)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
			continue;
		}

		// Append received chunk to accumulator
		Accum.Write(Buf.GetData(), Buf.GetWritePos());
		TrafficCounter.PushIncomingTraffic(Buf.GetWritePos());
		TrafficCounter.Refresh();
		DaemonToClientTrafficRate = TrafficCounter.TrafficItem->DownloadSpeed;

		// Try to parse as many frames as we can
		for (;;)
		{
			// Need 4 bytes for size prefix
			if (Accum.GetReadableSize() < 4)
				break;
			uint32_t FrameLength = 0;
			ReadU32(Accum.PeekReadPtr(), Accum.GetReadableSize(), FrameLength);

			// Sanity-check frame length to avoid huge allocations and potential
			// compiler/static-analysis array-bounds diagnostics (and to protect
			// against malformed/malicious data). Use a reasonable upper bound.
			constexpr uint32_t MAX_FRAME_LENGTH = 16 * 1024 * 1024; // 16 MiB
			if (FrameLength > MAX_FRAME_LENGTH)
			{
				spdlog::error("Received absurdly large frame length from server: {}", FrameLength);
				// Corrupt stream / desync: close socket to force reconnect and drop data
				Socket->Close();
				break; // break out of frame parsing loop and let connection loop handle reconnect
			}
			// Wait for full frame
			if (Accum.GetReadableSize() < 4 + FrameLength)
				break;

			// Create a buffer sized to the message payload to avoid calling
			// WBuffer::Resize (which can trigger inlined diagnostics). Construct
			// directly with the desired size.
			WBuffer Msg(FrameLength);
			// skip 4-byte prefix
			Accum.Consume(4);
			// copy payload into Msg
			std::memcpy(Msg.GetData(), Accum.PeekReadPtr(), FrameLength);
			Msg.SetWritingPos(FrameLength);
			// consume from accumulator
			Accum.Consume(FrameLength);

			// Dispatch based on 1-byte message type at start of payload
			auto Type = ReadMessageTypeFromBuffer(Msg);
			if (Type == MT_Invalid)
			{
				spdlog::error("Received invalid message type from server (framed)");
				continue;
			}
			switch (Type)
			{
				case MT_Handshake:
					HandleHandshake(Msg);
					break;
				case MT_TrafficTree:
					TrafficTree->LoadFromBuffer(Msg);
					break;
				case MT_TrafficTreeUpdate:
					TrafficTree->UpdateFromBuffer(Msg);
					break;
				case MT_AppIconAtlasData:
					WAppIconAtlas::GetInstance().FromAtlasData(Msg);
					break;
				case MT_ResolvedAddresses:
					TrafficTree->SetResolvedAddresses(Msg);
					break;
				default:
					spdlog::warn("Received unknown message type from server: {}", static_cast<int>(Type));
					break;
			}
		}
	}
}

bool WClient::EnsureConnected()
{
	if (Socket->GetState() == ES_Connected)
	{
		return true;
	}

	switch (Socket->GetState())
	{
		case ES_Initial:
			if (!Socket->Open())
			{
				return false;
			}
			// Fallthrough
		case ES_Opened:
			if (!Socket->Connect())
			{
				return false;
			}
		default:;
	}
	return Socket->GetState() == ES_Connected;
}

void WClient::HandleHandshake(WBuffer& Buf)
{

	WProtocolHandshake Handshake{};
	std::stringstream  ss;
	ss.write(Buf.GetData(), static_cast<long int>(Buf.GetWritePos()));
	{
		ss.seekg(1); // Skip message type
		cereal::BinaryInputArchive iar(ss);
		iar(Handshake);
	}

#if WDEBUG
	if (Handshake.ProtocolVersion != WAECHTER_PROTOCOL_VERSION || Handshake.CommitHash != GIT_COMMIT_HASH)
	{
		spdlog::error("Daemon protocol version ({}/{}) does not match client version ({}/{})",
			Handshake.ProtocolVersion, Handshake.CommitHash, WAECHTER_PROTOCOL_VERSION, GIT_COMMIT_HASH);
	}
#else
	if (Handshake.ProtocolVersion != WAECHTER_PROTOCOL_VERSION)
	{
		spdlog::warn("Daemon protocol version ({}) does not match client version ({})", Handshake.ProtocolVersion,
			WAECHTER_PROTOCOL_VERSION);
	}
#endif
	else
	{
		spdlog::info(
			"Connected to daemon (protocol version {}, commit {})", Handshake.ProtocolVersion, Handshake.CommitHash);
	}
}

void WClient::Start()
{
	Running = true;
	ConnectionThread = std::thread(&WClient::ConnectionThreadFunction, this);
}

WClient::WClient()
{
	Socket = std::make_shared<WClientSocket>("/var/run/waechterd.sock");
	TrafficTree = std::make_shared<WTrafficTree>();
}