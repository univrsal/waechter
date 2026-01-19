/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ClientWebSocket.hpp"

#include <cstring>
#include <spdlog/spdlog.h>

WClientWebSocket::WClientWebSocket(lws* Wsi_) : Wsi(Wsi_) {}

void WClientWebSocket::StartListenThread()
{
	// No separate thread needed - libwebsockets handles this in the server's service loop
}

bool WClientWebSocket::IsConnected() const
{
	return bConnected && Wsi != nullptr;
}

void WClientWebSocket::Close()
{
	bConnected = false;
	// The actual close is handled by libwebsockets
}

ssize_t WClientWebSocket::SendFramed(std::string const& Data)
{
	if (!IsConnected())
	{
		return -1;
	}

	// Prepare framed data with 4-byte length prefix
	auto const        Length = static_cast<uint32_t>(Data.size());
	std::vector<char> FramedData(LWS_PRE + sizeof(uint32_t) + Data.size());

	// Copy length prefix after LWS_PRE padding
	std::memcpy(FramedData.data() + LWS_PRE, &Length, sizeof(uint32_t));
	// Copy payload
	std::memcpy(FramedData.data() + LWS_PRE + sizeof(uint32_t), Data.data(), Data.size());

	{
		std::lock_guard Lock(SendMutex);
		SendQueue.push(std::move(FramedData));
	}

	RequestWrite();

	return static_cast<ssize_t>(sizeof(uint32_t) + Data.size());
}

void WClientWebSocket::HandleReceive(char const* Data, size_t Len)
{
	// Accumulate data (handle fragmented messages)
	ReceiveBuffer.insert(ReceiveBuffer.end(), Data, Data + Len);

	// Try to extract complete frames
	while (ReceiveBuffer.size() >= sizeof(uint32_t))
	{
		uint32_t FrameLength = 0;
		std::memcpy(&FrameLength, ReceiveBuffer.data(), sizeof(uint32_t));

		if (ReceiveBuffer.size() < sizeof(uint32_t) + FrameLength)
		{
			// Not enough data for complete frame
			break;
		}

		// Extract frame data (skip length prefix)
		WBuffer FrameBuffer(FrameLength);
		std::memcpy(FrameBuffer.GetData(), ReceiveBuffer.data() + sizeof(uint32_t), FrameLength);
		FrameBuffer.SetWritingPos(FrameLength);

		// Remove processed data from buffer
		ReceiveBuffer.erase(
			ReceiveBuffer.begin(), ReceiveBuffer.begin() + static_cast<ptrdiff_t>(sizeof(uint32_t) + FrameLength));

		// Emit the data signal
		OnData(FrameBuffer);
	}
}

void WClientWebSocket::HandleWritable()
{
	std::lock_guard Lock(SendMutex);

	if (SendQueue.empty() || !Wsi)
	{
		return;
	}

	auto& Front = SendQueue.front();
	// Data starts after LWS_PRE padding
	size_t DataLen = Front.size() - LWS_PRE;
	int Written = lws_write(Wsi, reinterpret_cast<unsigned char*>(Front.data() + LWS_PRE), DataLen, LWS_WRITE_BINARY);

	if (Written < 0)
	{
		spdlog::error("WebSocket write failed");
		bConnected = false;
		return;
	}

	SendQueue.pop();

	// If more data to send, request another write callback
	if (!SendQueue.empty())
	{
		lws_callback_on_writable(Wsi);
	}
}

void WClientWebSocket::HandleClose()
{
	bConnected = false;
	Wsi = nullptr;
	OnClosed();
}

void WClientWebSocket::RequestWrite()
{
	if (Wsi && bConnected)
	{
		lws_callback_on_writable(Wsi);
	}
}