//
// Created by usr on 27/10/2025.
//

#include "Client.hpp"

#include "AppIconAtlas.hpp"

#include <spdlog/spdlog.h>

#include "Messages.hpp"
#include <cstdint>

namespace
{
	// helper to read little-endian uint32 from bytes (we control both ends)
	static inline bool ReadU32(char const* p, size_t n, uint32_t& out)
	{
		if (n < 4)
			return false;
		uint32_t v;
		std::memcpy(&v, p, 4);
		out = v;
		return true;
	}
} // namespace

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
				case MT_TrafficTree:
					TrafficTree.LoadFromBuffer(Msg);
					break;
				case MT_TrafficTreeUpdate:
					TrafficTree.UpdateFromBuffer(Msg);
					break;
				case MT_AppIconAtlasData:
					WAppIconAtlas::GetInstance().FromAtlasData(Msg);
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

WClient::WClient()
{
}