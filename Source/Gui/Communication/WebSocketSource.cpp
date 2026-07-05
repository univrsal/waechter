/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "WebSocketSource.hpp"

#include "spdlog/spdlog.h"
#include "tracy/Tracy.hpp"

#include "Time.hpp"
#include "Util/Settings.hpp"

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
			Source->HandleReceive(
				static_cast<char*>(In), Len, lws_remaining_packet_payload(Wsi) == 0 && lws_is_final_fragment(Wsi));
			break;

		case LWS_CALLBACK_CLIENT_WRITEABLE:
			Source->HandleWritable();
			break;

		default:
			break;
	}

	return 0;
}

static lws_protocols const WWebSocketProtocols[] = {
	{ "waechter-protocol", ClientWebSocketCallback, 0, 65536, 0, nullptr, 0 }, { nullptr, nullptr, 0, 0, 0, nullptr, 0 }
};

static void EnsureLwsSSLInitialized()
{
	// We just keep this around so disconnecting from one daemon and connecting to another one
	// doesn't fail
	static lws_context* AnchorContext = [] {
		// Minimal protocol list - the anchor context never accepts or makes connections.
		static lws_protocols const NoProtocols[] = { { nullptr, nullptr, 0, 0, 0, nullptr, 0 } };

		lws_context_creation_info Info{};
		Info.port = CONTEXT_PORT_NO_LISTEN;
		Info.protocols = NoProtocols;
		Info.gid = static_cast<gid_t>(-1);
		Info.uid = static_cast<uid_t>(-1);
		// LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT makes lws increment lws_tls_shared_ref.
		// The anchor context is never destroyed, so the ref count never reaches 0
		// and OpenSSL global state is never torn down for the process lifetime.
		Info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

		lws_context* Ctx = lws_create_context(&Info);
		if (!Ctx)
			spdlog::warn(
				"WebSocket: failed to create SSL anchor context; switching between ws:// and wss:// daemons may fail");
		return Ctx;
	}();
	(void)AnchorContext;
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

	// Stop thread before destroying the context it may be using.
	WWebSocketSource::Stop();

	if (Context)
	{
		lws_context_destroy(Context);
		Context = nullptr;
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

void WWebSocketSource::HandleReceive(char const* Data, size_t Len, bool const bIsFinalFragment)
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
		lws_cancel_service(Context);
	}
}

void WWebSocketSource::ListenThreadFunction()
{
	tracy::SetThreadName("WebSocketClient");

	// Parse URL to extract host, port, and path
	std::string Host = "localhost";
	int         Port = 9001;
	std::string Path = "/";

	if (Url.starts_with("ws://") || Url.starts_with("wss://"))
	{
		std::string UrlRest = Url;
		if (Url.starts_with("ws://"))
		{
			UrlRest = Url.substr(5);
			Port = 80;
		}
		else if (Url.starts_with("wss://"))
		{
			UrlRest = Url.substr(6);
			Port = 443;
		}
		auto ColonPos = UrlRest.find(':');
		auto SlashPos = UrlRest.find('/');

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
	bool const bIsSecure = Url.starts_with("wss://");
	spdlog::info("Connecting to WebSocket: {}:{}{} (secure: {})", Host, Port, Path, bIsSecure);

	if (!AuthToken.empty())
	{
		if (Path.find('?') != std::string::npos)
		{
			Path += "&token=" + AuthToken;
		}
		else
		{
			Path += "?token=" + AuthToken;
		}
	}

	if (!Context)
	{
		spdlog::error("WebSocket context is null - was Start() called?");
		bListenThreadRunning = false;
		return;
	}

	lws_client_connect_info ConnectInfo{};
	ConnectInfo.context = Context;
	ConnectInfo.address = Host.c_str();
	ConnectInfo.port = Port;
	ConnectInfo.path = Path.c_str();
	ConnectInfo.host = Host.c_str();
	ConnectInfo.origin = Host.c_str();
	ConnectInfo.protocol = WWebSocketProtocols[0].name;
	ConnectInfo.ietf_version_or_minus_one = -1;
	ConnectInfo.userdata = this;

	if (bIsSecure)
	{
		ConnectInfo.ssl_connection = LCCSCF_USE_SSL;
		if (WSettings::GetInstance().bAllowSelfSignedCertificates)
		{
			ConnectInfo.ssl_connection |= LCCSCF_ALLOW_SELFSIGNED;
		}
		if (WSettings::GetInstance().bSkipCertificateHostnameCheck)
		{
			ConnectInfo.ssl_connection |= LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
		}
	}

	Wsi = lws_client_connect_via_info(&ConnectInfo);
	if (!Wsi)
	{
		spdlog::error("Failed to initiate WebSocket connection");
		bListenThreadRunning = false;
		return;
	}

	while (bListenThreadRunning && Context)
	{
		lws_service(Context, 50);

		if (bConnectionFailed)
		{
			break;
		}
	}

	// Do NOT destroy Context here - it is kept alive for the next connection.
	Wsi = nullptr;
	bConnected = false;
	bListenThreadRunning = false;
}

void WWebSocketSource::Start()
{
	EnsureLwsSSLInitialized();

	// Create the lws context once
	if (!Context)
	{
		lws_set_log_level(LLL_ERR | LLL_WARN, nullptr);

		lws_context_creation_info Info{};
		Info.port = CONTEXT_PORT_NO_LISTEN;
		Info.protocols = WWebSocketProtocols;
		Info.gid = static_cast<gid_t>(-1);
		Info.uid = static_cast<uid_t>(-1);
		// Always initialise SSL globally so the state is valid for both ws and wss connections.
		Info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
		if (WSettings::GetInstance().bAllowSelfSignedCertificates)
		{
			Info.options |= LWS_SERVER_OPTION_PEER_CERT_NOT_REQUIRED;
		}

		Context = lws_create_context(&Info);
		if (!Context)
		{
			spdlog::error("Failed to create libwebsockets context");
			return;
		}
	}

	if (!bListenThreadRunning)
	{
		if (ListenerThread.joinable())
			ListenerThread.join();
		bConnectionFailed = false;
		bListenThreadRunning = true;
		ListenerThread = std::thread(&WWebSocketSource::ListenThreadFunction, this);
	}

	// Set up reconnection timer (only if not already active).
	if (TimerId == -1)
	{
		TimerId = WTimerManager::GetInstance().AddTimer(5.0, [this] {
			if (!IsConnected() && !bListenThreadRunning)
			{
				spdlog::info("Attempting to reconnect WebSocket...");
				bConnectionFailed = false;
				bListenThreadRunning = true;
				if (ListenerThread.joinable())
					ListenerThread.join();
				ListenerThread = std::thread(&WWebSocketSource::ListenThreadFunction, this);
			}
		});
	}
}

void WWebSocketSource::Stop()
{
	if (TimerId != -1)
	{
		WTimerManager::GetInstance().RemoveTimer(TimerId);
		TimerId = -1;
	}

	bListenThreadRunning = false;
	bConnected = false;

	// Wake up lws_service() so the thread can exit promptly.
	if (Context)
	{
		lws_cancel_service(Context);
	}

	if (ListenerThread.joinable())
	{
		ListenerThread.join();
	}
	// Context is intentionally NOT destroyed here.
	// We keep it alive so that the next Start() can reuse it
	// otherwise subsequent new connection attempts might fail
}

bool WWebSocketSource::SendFramed(std::string const& Data)
{
	if (!bConnected || !Wsi)
	{
		return false;
	}

	if (Data.empty())
	{
		spdlog::error("WebSocket send data is empty");
		return false;
	}

	// WebSocket already preserves message boundaries, so send the serialized payload directly.
	std::vector<char> FramedData(LWS_PRE + Data.size());
	std::memcpy(FramedData.data() + LWS_PRE, Data.data(), Data.size());

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