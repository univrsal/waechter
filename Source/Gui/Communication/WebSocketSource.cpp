/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "WebSocketSource.hpp"

#include <spdlog/spdlog.h>

#include "Time.hpp"

WWebSocketSource::WWebSocketSource(std::string WebSocketUrl) : Url(std::move(WebSocketUrl))
{
	curl_global_init(CURL_GLOBAL_DEFAULT);
}

WWebSocketSource::~WWebSocketSource()
{
	Stop();
	curl_global_cleanup();
}

bool WWebSocketSource::Connect()
{
	std::lock_guard lk(CurlMutex);

	if (Curl)
	{
		curl_easy_cleanup(Curl);
		Curl = nullptr;
	}

	Curl = curl_easy_init();
	if (!Curl)
	{
		spdlog::error("Failed to initialize curl");
		return false;
	}

	curl_easy_setopt(Curl, CURLOPT_URL, Url.c_str());
	curl_easy_setopt(Curl, CURLOPT_CONNECT_ONLY, 2L); // WebSocket mode

	CURLcode Res = curl_easy_perform(Curl);
	if (Res != CURLE_OK)
	{
		spdlog::error("WebSocket connection failed: {}", curl_easy_strerror(Res));
		curl_easy_cleanup(Curl);
		Curl = nullptr;
		return false;
	}

	bConnected = true;
	spdlog::info("WebSocket connected to {}", Url);
	return true;
}

void WWebSocketSource::ListenThreadFunction()
{
	constexpr size_t BufferSize = 8192;
	char             Buffer[BufferSize];

	while (bListenThreadRunning && bConnected)
	{
		size_t               Received = 0;
		curl_ws_frame const* Meta = nullptr;

		CURLcode Res;
		{
			std::lock_guard lk(CurlMutex);
			if (!Curl)
			{
				break;
			}
			Res = curl_ws_recv(Curl, Buffer, BufferSize, &Received, &Meta);
		}

		if (Res == CURLE_AGAIN)
		{
			// No data available, wait a bit and try again
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			continue;
		}

		if (Res != CURLE_OK)
		{
			if (Res == CURLE_GOT_NOTHING || Res == CURLE_RECV_ERROR)
			{
				spdlog::info("WebSocket connection closed");
			}
			else
			{
				spdlog::error("WebSocket receive error: {}", curl_easy_strerror(Res));
			}
			bConnected = false;
			OnClosed();
			break;
		}

		if (Meta && (Meta->flags & CURLWS_CLOSE))
		{
			spdlog::info("WebSocket received close frame");
			bConnected = false;
			OnClosed();
			break;
		}

		if (Received > 0 && Meta && (Meta->flags & CURLWS_BINARY))
		{
			// WebSocket is already framed, so each complete message is a frame
			// The data should already contain the 4-byte length prefix from the daemon
			if (Received >= sizeof(uint32_t))
			{
				uint32_t FrameLength = 0;
				std::memcpy(&FrameLength, Buffer, sizeof(uint32_t));

				if (Received >= sizeof(uint32_t) + FrameLength)
				{
					// Create buffer with frame data (skip length prefix)
					WBuffer FrameBuffer(FrameLength);
					std::memcpy(FrameBuffer.GetData(), Buffer + sizeof(uint32_t), FrameLength);
					FrameBuffer.SetWritingPos(FrameLength);

					// Emit the data signal
					OnData(FrameBuffer);
				}
			}
		}
	}

	bListenThreadRunning = false;
}

void WWebSocketSource::Start()
{
	if (Connect())
	{
		if (!bListenThreadRunning)
		{
			if (ListenerThread.joinable())
			{
				ListenerThread.join();
			}
			bListenThreadRunning = true;
			ListenerThread = std::thread(&WWebSocketSource::ListenThreadFunction, this);
		}
	}

	// Set up reconnection timer similar to UnixSocketSource
	WTimerManager::GetInstance().AddTimer(0.5, [this] {
		if (!IsConnected() && !bListenThreadRunning)
		{
			if (Connect())
			{
				bListenThreadRunning = true;
				if (ListenerThread.joinable())
				{
					ListenerThread.join();
				}
				ListenerThread = std::thread(&WWebSocketSource::ListenThreadFunction, this);
			}
		}
	});
}

void WWebSocketSource::Stop()
{
	bListenThreadRunning = false;
	bConnected = false;

	{
		std::lock_guard lk(CurlMutex);
		if (Curl)
		{
			curl_easy_cleanup(Curl);
			Curl = nullptr;
		}
	}

	if (ListenerThread.joinable())
	{
		ListenerThread.join();
	}

	OnClosed();
}

bool WWebSocketSource::SendFramed(std::string const& Data)
{
	std::lock_guard lk(CurlMutex);

	if (!Curl || !bConnected)
	{
		return false;
	}

	// Prepare framed data with 4-byte length prefix
	uint32_t const    Length = static_cast<uint32_t>(Data.size());
	std::vector<char> FramedData(sizeof(uint32_t) + Data.size());
	std::memcpy(FramedData.data(), &Length, sizeof(uint32_t));
	std::memcpy(FramedData.data() + sizeof(uint32_t), Data.data(), Data.size());

	size_t   Sent = 0;
	CURLcode Res = curl_ws_send(Curl, FramedData.data(), FramedData.size(), &Sent, 0, CURLWS_BINARY);

	if (Res != CURLE_OK)
	{
		spdlog::error("WebSocket send failed: {}", curl_easy_strerror(Res));
		return false;
	}

	return Sent == FramedData.size();
}

bool WWebSocketSource::IsConnected() const
{
	return bConnected;
}