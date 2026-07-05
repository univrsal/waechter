/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ClientWebSocket.hpp"

#include <cstring>

#include "spdlog/spdlog.h"

WClientWebSocket::WClientWebSocket(lws* Wsi_, lws_context* Context_) : Wsi(Wsi_), Context(Context_) {}

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

	if (Data.empty())
	{
		spdlog::error("WebSocket send data is empty");
		return -1;
	}

	// WebSocket already preserves message boundaries, so send the serialized payload directly.
	std::vector<char> FramedData(LWS_PRE + Data.size());
	std::memcpy(FramedData.data() + LWS_PRE, Data.data(), Data.size());

	{
		std::lock_guard Lock(SendMutex);
		SendQueue.push(std::move(FramedData));
	}

	RequestWrite();

	return static_cast<ssize_t>(Data.size());
}

void WClientWebSocket::HandleReceive(char const* Data, size_t Len, bool const bIsFinalFragment)
{
	ReceiveBuffer.insert(ReceiveBuffer.end(), Data, Data + Len);
	if (!bIsFinalFragment)
	{
		return;
	}

	WBuffer FrameBuffer(ReceiveBuffer.size());
	FrameBuffer.Write(std::span<char const>(ReceiveBuffer.data(), ReceiveBuffer.size()));
	ReceiveBuffer.clear();
	OnData(FrameBuffer);
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
		// Wake up the service loop to process the write callback immediately
		if (Context)
		{
			lws_cancel_service(Context);
		}
	}
}