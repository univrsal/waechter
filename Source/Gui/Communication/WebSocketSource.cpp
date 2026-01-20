/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "WebSocketSource.hpp"

#include <spdlog/spdlog.h>
#include <tracy/Tracy.hpp>

#include "Time.hpp"

static int ClientWebSocketCallback(
	[[maybe_unused]] lws* Wsi, lws_callback_reasons Reason, void* User, void* In, size_t Len)
{
	auto* Source = static_cast<WWebSocketSource*>(User);
	if (!Source)
	{
		return 0;
	}

	switch (Reason)
	{
		case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
		{
			auto**      P = static_cast<unsigned char**>(In);
			auto*       End = *P + Len;
			std::string AuthHeader = "Authorization: Bearer " + Source->GetAuthToken() + "\x0d\x0a";

			if (End - *P < static_cast<int>(AuthHeader.size()))
			{
				spdlog::error("Not enough space to add Authorization header");
				return -1;
			}

			std::memcpy(*P, AuthHeader.data(), AuthHeader.size());
			*P += AuthHeader.size();
			break;
		}

		case LWS_CALLBACK_CLIENT_ESTABLISHED:
			spdlog::info("WebSocket client connected");
			Source->OnConnected();
			break;

		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			spdlog::error("WebSocket connection error: {}", In ? static_cast<char*>(In) : "unknown");
			Source->OnConnectionError();
			break;

		case LWS_CALLBACK_CLIENT_CLOSED:
			spdlog::info("WebSocket connection closed");
			Source->OnConnectionClosed();
			break;

		case LWS_CALLBACK_CLIENT_RECEIVE:
			Source->HandleReceive(static_cast<char*>(In), Len);
			break;

		case LWS_CALLBACK_CLIENT_WRITEABLE:
			Source->HandleWritable();
			break;

		default:
			break;
	}

	return 0;
}

WWebSocketSource::WWebSocketSource(std::string WebSocketUrl, std::string AuthToken_)
	: Url(std::move(WebSocketUrl)), AuthToken(std::move(AuthToken_))
{
}

WWebSocketSource::~WWebSocketSource()
{
	if (TimerId != -1)
	{
		WTimerManager::GetInstance().RemoveTimer(TimerId);
		TimerId = -1;
	}
}

void WWebSocketSource::OnConnected()
{
	bConnected = true;
}

void WWebSocketSource::OnConnectionError()
{
	bConnected = false;
	bConnectionFailed = true;
}

void WWebSocketSource::OnConnectionClosed()
{
	bConnected = false;
	OnClosed();
}

void WWebSocketSource::HandleReceive(char const* Data, size_t Len)
{
	// Accumulate received data
	ReceiveBuffer.insert(ReceiveBuffer.end(), Data, Data + Len);

	// Process complete frames (with 4-byte length prefix)
	while (ReceiveBuffer.size() >= sizeof(uint32_t))
	{
		uint32_t FrameLength = 0;
		std::memcpy(&FrameLength, ReceiveBuffer.data(), sizeof(uint32_t));

		if (ReceiveBuffer.size() >= sizeof(uint32_t) + FrameLength)
		{
			// Create buffer with frame data (skip length prefix)
			WBuffer FrameBuffer(FrameLength);
			std::memcpy(FrameBuffer.GetData(), ReceiveBuffer.data() + sizeof(uint32_t), FrameLength);
			FrameBuffer.SetWritingPos(FrameLength);

			// Remove processed frame from buffer
			ReceiveBuffer.erase(ReceiveBuffer.begin(), ReceiveBuffer.begin() + sizeof(uint32_t) + FrameLength);

			// Emit the data signal
			OnData(FrameBuffer);
		}
		else
		{
			// Wait for more data
			break;
		}
	}
}

void WWebSocketSource::HandleWritable()
{
	std::lock_guard Lock(SendMutex);

	if (SendQueue.empty())
	{
		return;
	}

	auto& Message = SendQueue.front();

	// Send with binary frame
	int Sent = lws_write(
		Wsi, reinterpret_cast<unsigned char*>(Message.data()) + LWS_PRE, Message.size() - LWS_PRE, LWS_WRITE_BINARY);

	if (Sent >= 0 && static_cast<size_t>(Sent) == Message.size() - LWS_PRE)
	{
		SendQueue.pop();
	}
	else
	{
		spdlog::error("WebSocket partial send: {} of {} bytes", Sent, Message.size() - LWS_PRE);
	}

	// Request another write callback if more messages are queued
	if (!SendQueue.empty())
	{
		lws_callback_on_writable(Wsi);
	}
}

void WWebSocketSource::RequestWrite() const
{
	if (Wsi && Context)
	{
		lws_callback_on_writable(Wsi);
	}
}

void WWebSocketSource::ListenThreadFunction()
{
	tracy::SetThreadName("WebSocketClient");

	// Parse URL to extract host, port, and path
	std::string Host = "localhost";
	int         Port = 9001;
	std::string Path = "/";

	if (Url.starts_with("ws://"))
	{
		std::string UrlRest = Url.substr(5); // Skip "ws://"
		auto        ColonPos = UrlRest.find(':');
		auto        SlashPos = UrlRest.find('/');

		if (ColonPos != std::string::npos)
		{
			Host = UrlRest.substr(0, ColonPos);
			if (SlashPos != std::string::npos)
			{
				Port = std::stoi(UrlRest.substr(ColonPos + 1, SlashPos - ColonPos - 1));
				Path = UrlRest.substr(SlashPos);
			}
			else
			{
				Port = std::stoi(UrlRest.substr(ColonPos + 1));
			}
		}
		else if (SlashPos != std::string::npos)
		{
			Host = UrlRest.substr(0, SlashPos);
			Path = UrlRest.substr(SlashPos);
		}
		else
		{
			Host = UrlRest;
		}
	}

	spdlog::info("Connecting to WebSocket: {}:{}{}", Host, Port, Path);

	// Set up libwebsockets logging to use spdlog
	lws_set_log_level(LLL_ERR | LLL_WARN, nullptr);

	static lws_protocols Protocols[] = { { "waechter-protocol", ClientWebSocketCallback, 0, 65536, 0, this, 0 },
		{ nullptr, nullptr, 0, 0, 0, nullptr, 0 } };

	lws_context_creation_info Info{};
	Info.port = CONTEXT_PORT_NO_LISTEN;
	Info.protocols = Protocols;
	Info.gid = static_cast<gid_t>(-1);
	Info.uid = static_cast<uid_t>(-1);
	Info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

	Context = lws_create_context(&Info);
	if (!Context)
	{
		spdlog::error("Failed to create libwebsockets context");
		return;
	}

	lws_client_connect_info ConnectInfo{};
	ConnectInfo.context = Context;
	ConnectInfo.address = Host.c_str();
	ConnectInfo.port = Port;
	ConnectInfo.path = Path.c_str();
	ConnectInfo.host = Host.c_str();
	ConnectInfo.origin = Host.c_str();
	ConnectInfo.protocol = Protocols[0].name;
	ConnectInfo.ietf_version_or_minus_one = -1;
	ConnectInfo.userdata = this;

	Wsi = lws_client_connect_via_info(&ConnectInfo);
	if (!Wsi)
	{
		spdlog::error("Failed to initiate WebSocket connection");
		lws_context_destroy(Context);
		Context = nullptr;
		return;
	}

	while (bListenThreadRunning && Context)
	{
		lws_service(Context, 50);

		// Check for connection failure
		if (bConnectionFailed)
		{
			break;
		}
	}

	if (Context)
	{
		lws_context_destroy(Context);
		Context = nullptr;
		Wsi = nullptr;
	}

	bConnected = false;
	bListenThreadRunning = false;
}

void WWebSocketSource::Start()
{
	if (!bListenThreadRunning)
	{
		if (ListenerThread.joinable())
		{
			ListenerThread.join();
		}
		bConnectionFailed = false;
		bListenThreadRunning = true;
		ListenerThread = std::thread(&WWebSocketSource::ListenThreadFunction, this);
	}

	// Set up reconnection timer
	TimerId = WTimerManager::GetInstance().AddTimer(2.0, [this] {
		if (!IsConnected() && !bListenThreadRunning)
		{
			spdlog::info("Attempting to reconnect WebSocket...");
			bConnectionFailed = false;
			bListenThreadRunning = true;
			if (ListenerThread.joinable())
			{
				ListenerThread.join();
			}
			ListenerThread = std::thread(&WWebSocketSource::ListenThreadFunction, this);
		}
	});
}

void WWebSocketSource::Stop()
{

	bListenThreadRunning = false;
	bConnected = false;

	// Cancel the service to wake up lws_service()
	if (Context)
	{
		lws_cancel_service(Context);
	}

	if (ListenerThread.joinable())
	{
		ListenerThread.join();
	}
}

bool WWebSocketSource::SendFramed(std::string const& Data)
{
	if (!bConnected || !Wsi)
	{
		return false;
	}

	if (Data.size() > UINT32_MAX)
	{
		spdlog::error("WebSocket send data too large: {} bytes", Data.size());
		return false;
	}

	if (Data.empty())
	{
		spdlog::error("WebSocket send data is empty");
		return false;
	}

	// Prepare framed data with 4-byte length prefix and LWS_PRE padding
	auto const        Length = static_cast<uint32_t>(Data.size());
	std::vector<char> FramedData(LWS_PRE + sizeof(uint32_t) + Data.size());

	std::memcpy(FramedData.data() + LWS_PRE, &Length, sizeof(uint32_t));
	std::memcpy(FramedData.data() + LWS_PRE + sizeof(uint32_t), Data.data(), Data.size());

	{
		std::lock_guard Lock(SendMutex);
		SendQueue.push(std::move(FramedData));
	}

	// Request write callback
	RequestWrite();

	return true;
}

bool WWebSocketSource::IsConnected() const
{
	return bConnected;
}