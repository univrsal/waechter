/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "DaemonWebSocket.hpp"

#include "spdlog/spdlog.h"
#include "tracy/Tracy.hpp"

#include "ClientWebSocket.hpp"
#include "DaemonClient.hpp"
#include "DaemonConfig.hpp"

void LwsLogCallback(int Level, char const* Line)
{
	std::string Msg(Line);
	while (!Msg.empty() && (Msg.back() == '\n' || Msg.back() == '\r'))
	{
		Msg.pop_back();
	}

	// Strip libwebsockets timestamp prefix (format: [YYYY/MM/DD HH:MM:SS:FFFF] )
	if (Msg.size() > 30)
	{
		Msg = Msg.substr(30);
	}

	switch (Level)
	{
		case LLL_ERR:
			spdlog::error("{}", Msg);
			break;
		case LLL_WARN:
			spdlog::warn("{}", Msg);
			break;
		case LLL_NOTICE:
		case LLL_INFO:
			spdlog::info("{}", Msg);
			break;
		case LLL_DEBUG:
		case LLL_PARSER:
		case LLL_HEADER:
		case LLL_EXT:
		case LLL_CLIENT:
		case LLL_LATENCY:
		default:
			spdlog::debug("{}", Msg);
			break;
	}
}

int WebSocketCallback(lws* Wsi, lws_callback_reasons Reason, void*, void* In, size_t Len)
{
	spdlog::debug("WebSocket callback triggered: reason={}", static_cast<int>(Reason));

	// Get the context and retrieve our server pointer
	lws_context* Context = lws_get_context(Wsi);
	auto*        DaemonWebSocket = static_cast<WDaemonWebSocket*>(lws_context_user(Context));

	if (!DaemonWebSocket)
	{
		spdlog::warn("WebSocket callback: DaemonWebSocket pointer is null");
		return 0;
	}

	switch (Reason)
	{
		case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
		{
			// Check authentication before connection is established
			auto const& Cfg = WDaemonConfig::GetInstance();
			bool        Authenticated = false;

			// Try to get Authorization header first (for native clients)
			char AuthHeaderBuf[256];
			int  HeaderLen = lws_hdr_copy(Wsi, AuthHeaderBuf, sizeof(AuthHeaderBuf), WSI_TOKEN_HTTP_AUTHORIZATION);

			if (HeaderLen > 0)
			{
				std::string AuthHeader(AuthHeaderBuf, static_cast<size_t>(HeaderLen));
				std::string ExpectedAuth = "Bearer " + Cfg.WebSocketAuthToken;

				if (AuthHeader == ExpectedAuth)
				{
					Authenticated = true;
					spdlog::debug("WebSocket connection authenticated via header");
				}
			}

			// If header auth failed, try query parameter (for browser clients)
			if (!Authenticated)
			{
				char QueryBuf[512];
				int  QueryLen = lws_hdr_copy(Wsi, QueryBuf, sizeof(QueryBuf), WSI_TOKEN_HTTP_URI_ARGS);

				if (QueryLen > 0)
				{
					std::string QueryString(QueryBuf, static_cast<size_t>(QueryLen));
					spdlog::debug("WebSocket query string: {}", QueryString);

					// Parse query parameters (format: token=value&other=value)
					size_t TokenPos = QueryString.find("token=");
					if (TokenPos != std::string::npos)
					{
						size_t      TokenStart = TokenPos + 6; // Skip "token="
						size_t      TokenEnd = QueryString.find('&', TokenStart);
						std::string Token = (TokenEnd != std::string::npos)
							? QueryString.substr(TokenStart, TokenEnd - TokenStart)
							: QueryString.substr(TokenStart);

						if (Token == Cfg.WebSocketAuthToken)
						{
							Authenticated = true;
							spdlog::debug("WebSocket connection authenticated via query parameter");
						}
						else
						{
							spdlog::warn("WebSocket connection rejected: invalid token in query parameter");
						}
					}
				}
			}

			if (!Authenticated)
			{
				spdlog::warn("WebSocket connection rejected: no valid authentication found");
				return -1; // Reject connection
			}

			break;
		}
		case LWS_CALLBACK_ESTABLISHED:
		{
			spdlog::info("WebSocket client connected");
			lws_context* NewContext = lws_get_context(Wsi);
			auto         ClientWs = std::make_shared<WClientWebSocket>(Wsi, NewContext);
			DaemonWebSocket->RegisterClient(Wsi, ClientWs);

			auto NewClient = std::make_shared<WDaemonClient>(ClientWs);
			DaemonWebSocket->GetNewConnectionSignal()(NewClient);
			break;
		}

		case LWS_CALLBACK_CLOSED:
		{
			spdlog::info("WebSocket client disconnected");
			if (auto Client = DaemonWebSocket->GetClient(Wsi))
			{
				Client->HandleClose();
			}
			DaemonWebSocket->UnregisterClient(Wsi);
			break;
		}

		case LWS_CALLBACK_RECEIVE:
		{
			if (auto Client = DaemonWebSocket->GetClient(Wsi))
			{
				Client->HandleReceive(static_cast<char*>(In), Len);
			}
			break;
		}

		case LWS_CALLBACK_SERVER_WRITEABLE:
		{
			if (auto Client = DaemonWebSocket->GetClient(Wsi))
			{
				Client->HandleWritable();
			}
			break;
		}

		default:
			break;
	}

	return 0;
}

void WDaemonWebSocket::ListenThreadFunction() const
{
	tracy::SetThreadName("WebSocketServer");
	pthread_setname_np(pthread_self(), "ws-server");

	while (Running && Context)
	{
		lws_service(Context, 10); // 10ms timeout for processing
	}
}

WDaemonWebSocket::WDaemonWebSocket() = default;

WDaemonWebSocket::~WDaemonWebSocket()
{
	Stop();
}

bool WDaemonWebSocket::StartListenThread()
{
	auto const& Cfg = WDaemonConfig::GetInstance();

	assert(!Cfg.WebSocketAuthToken.empty());

	// Redirect libwebsockets logging to spdlog
	lws_set_log_level(LLL_ERR | LLL_WARN, LwsLogCallback);

	// Parse port from the socket path (format: ws://host:port or just port number)
	int         Port = 9876; // default port
	std::string SocketPath = Cfg.DaemonSocketPath;

	// Try to extract port from ws:// URL
	if (SocketPath.starts_with("ws://"))
	{
		auto ColonPos = SocketPath.rfind(':');
		if (ColonPos != std::string::npos && ColonPos > 5)
		{
			try
			{
				Port = std::stoi(SocketPath.substr(ColonPos + 1));
			}
			catch (...)
			{
				spdlog::warn("Failed to parse port from {}, using default {}", SocketPath, Port);
			}
		}
	}

	static lws_protocols Protocols[] = { // Default protocol (no subprotocol specified)
		{ "waechter-protocol", WebSocketCallback, 0, 65536, 0, nullptr, 0 },
		// Null terminator
		{ nullptr, nullptr, 0, 0, 0, nullptr, 0 }
	};

	lws_context_creation_info Info{};
	Info.port = Port;
	Info.protocols = Protocols;
	Info.user = this; // Set the user pointer to this instance
	Info.gid = static_cast<gid_t>(-1);
	Info.uid = static_cast<uid_t>(-1);
	Info.options = LWS_SERVER_OPTION_VALIDATE_UTF8;

	Context = lws_create_context(&Info);
	if (!Context)
	{
		spdlog::error("Failed to create libwebsockets context");
		return false;
	}

	Running = true;
	ListenThread = std::thread(&WDaemonWebSocket::ListenThreadFunction, this);

	spdlog::info("WebSocket server started on port {}", Port);
	return true;
}

void WDaemonWebSocket::Stop()
{
	Running = false;

	// Wake up lws_service() so it can exit promptly
	if (Context)
	{
		lws_cancel_service(Context);
	}

	if (ListenThread.joinable())
	{
		ListenThread.join();
	}

	if (Context)
	{
		lws_context_destroy(Context);
		Context = nullptr;
	}

	std::lock_guard Lock(ClientsMutex);
	Clients.clear();
}

void WDaemonWebSocket::RegisterClient(lws* Wsi, std::shared_ptr<WClientWebSocket> Client)
{
	std::lock_guard Lock(ClientsMutex);
	Clients[Wsi] = std::move(Client);
}

void WDaemonWebSocket::UnregisterClient(lws* Wsi)
{
	std::lock_guard Lock(ClientsMutex);
	Clients.erase(Wsi);
}

std::shared_ptr<WClientWebSocket> WDaemonWebSocket::GetClient(lws* Wsi)
{
	std::lock_guard Lock(ClientsMutex);
	auto            It = Clients.find(Wsi);
	if (It != Clients.end())
	{
		return It->second;
	}
	return nullptr;
}