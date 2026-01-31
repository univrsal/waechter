/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "EmscriptenSocketSource.hpp"

#include <emscripten/bind.h>

#include "spdlog/spdlog.h"

#include "Time.hpp"

#include <emscripten/em_macros.h>

// Global callback handlers bound to C++
namespace
{
	WEmscriptenSocketSource* GSocketInstance = nullptr;

	void OnWebSocketOpen()
	{
		if (GSocketInstance)
		{
			GSocketInstance->OnConnected();
		}
	}

	void OnWebSocketError()
	{
		if (GSocketInstance)
		{
			GSocketInstance->OnConnectionError();
		}
	}

	void OnWebSocketClose()
	{
		if (GSocketInstance)
		{
			GSocketInstance->OnConnectionClosed();
		}
	}
} // namespace

WEmscriptenSocketSource::WEmscriptenSocketSource(std::string WebSocketUrl, std::string AuthToken_)
	: Url(std::move(WebSocketUrl)), AuthToken(std::move(AuthToken_))
{
}

WEmscriptenSocketSource::~WEmscriptenSocketSource() = default;

void WEmscriptenSocketSource::OnConnected()
{
	bConnected = true;
	bConnectionFailed = false;
	spdlog::info("Emscripten WebSocket connected");
}

void WEmscriptenSocketSource::OnConnectionError()
{
	bConnected = false;
	bConnectionFailed = true;
	spdlog::error("Emscripten WebSocket connection error");
}

void WEmscriptenSocketSource::OnConnectionClosed()
{
	bConnected = false;
	OnClosed();
	spdlog::info("Emscripten WebSocket connection closed");
}

void WEmscriptenSocketSource::HandleReceive(char const* Data, size_t Len)
{
	ReceiveBuffer.insert(ReceiveBuffer.end(), Data, Data + Len);

	// Process complete frames (with 4-byte length prefix)
	while (ReceiveBuffer.size() >= sizeof(uint32_t))
	{
		uint32_t FrameLength = 0;
		std::memcpy(&FrameLength, ReceiveBuffer.data(), sizeof(uint32_t));

		if (ReceiveBuffer.size() >= sizeof(uint32_t) + FrameLength)
		{
			WBuffer FrameBuffer(FrameLength);
			std::memcpy(FrameBuffer.GetData(), ReceiveBuffer.data() + sizeof(uint32_t), FrameLength);
			FrameBuffer.SetWritingPos(FrameLength);

			ReceiveBuffer.erase(ReceiveBuffer.begin(), ReceiveBuffer.begin() + sizeof(uint32_t) + FrameLength);
			OnData(FrameBuffer);
		}
		else
		{
			// Wait for more data
			break;
		}
	}
}

void WEmscriptenSocketSource::Start()
{
	if (bConnected)
	{
		return;
	}

	GSocketInstance = this;

	std::string BrowserUrl = Url;
	if (BrowserUrl.starts_with("ws://") || BrowserUrl.starts_with("wss://"))
	{
		// URL is already in the correct format
	}
	else if (BrowserUrl.starts_with("http://"))
	{
		BrowserUrl.replace(0, 7, "ws://");
	}
	else if (BrowserUrl.starts_with("https://"))
	{
		BrowserUrl.replace(0, 8, "wss://");
	}

	// Add auth token as the query parameter (browsers can't set custom headers in WebSocket handshake)
	spdlog::info("Connecting to Emscripten WebSocket: {}", BrowserUrl);
	if (!AuthToken.empty())
	{
		if (BrowserUrl.find('?') != std::string::npos)
		{
			BrowserUrl += "&token=" + AuthToken;
		}
		else
		{
			BrowserUrl += "?token=" + AuthToken;
		}
	}

	int Result = EM_ASM_INT(
		{
			try
			{
				var url = UTF8ToString($0);
				console.log('Creating WebSocket connection to:', url);

				var ws = new WebSocket(url, 'waechter-protocol');
				ws.binaryType = 'arraybuffer';

				ws.onopen = function()
				{
					console.log('WebSocket opened successfully');
					Module._emscripten_websocket_on_open();
				};

				ws.onerror = function(err)
				{
					console.error('WebSocket error event:', err);
					Module._emscripten_websocket_on_error();
				};

				ws.onclose = function(event)
				{
					console.log(
						'WebSocket closed - Code:', event.code, 'Reason:', event.reason, 'Clean:', event.wasClean);
					Module._emscripten_websocket_on_close();
				};

				ws.onmessage = function(event)
				{
					// Convert ArrayBuffer to Uint8Array
					var dataView = new Uint8Array(event.data);
					var length = dataView.length;

					// Write directly to heap (requires malloc/free to be exported)
					var ptr = _malloc(length);
					if (ptr)
					{
						HEAPU8.set(dataView, ptr);
						_emscripten_websocket_on_message_js(ptr, length);
						_free(ptr);
					}
				};

				// Store in Module for later access
				Module['waechter_websocket'] = ws;

				return 1;
			}
			catch (e)
			{
				console.error('Failed to create WebSocket:', e.message, e.stack);
				return 0;
			}
		},
		BrowserUrl.c_str());

	if (Result == 0)
	{
		spdlog::error("Failed to create WebSocket");
		bConnectionFailed = true;
		return;
	}

	WebSocket = emscripten::val::module_property("waechter_websocket");
}

void WEmscriptenSocketSource::Stop()
{
	if (!WebSocket.isNull())
	{
		try
		{
			int ReadyState = WebSocket["readyState"].as<int>();
			// OPEN = 1, CONNECTING = 0
			if (ReadyState == 0 || ReadyState == 1)
			{
				WebSocket.call<void>("close");
			}
		}
		catch (emscripten::val const& e)
		{
			spdlog::error("Error closing WebSocket: {}", e["message"].as<std::string>());
		}

		WebSocket = emscripten::val::null();
	}

	bConnected = false;
	GSocketInstance = nullptr;
}

bool WEmscriptenSocketSource::SendFramed(std::string const& Data)
{
	if (!bConnected || WebSocket.isNull())
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

	try
	{
		auto const           Length = static_cast<uint32_t>(Data.size());
		std::vector<uint8_t> FramedData(sizeof(uint32_t) + Data.size());

		std::memcpy(FramedData.data(), &Length, sizeof(uint32_t));
		std::memcpy(FramedData.data() + sizeof(uint32_t), Data.data(), Data.size());

		auto ArrayBuf = emscripten::val::global("Uint8Array")
							.new_(emscripten::val(emscripten::typed_memory_view(FramedData.size(), FramedData.data())));
		WebSocket.call<void>("send", ArrayBuf);

		return true;
	}
	catch (emscripten::val const& e)
	{
		spdlog::error("WebSocket send error: {}", e["message"].as<std::string>());
		return false;
	}
}

bool WEmscriptenSocketSource::IsConnected() const
{
	return bConnected;
}

// Emscripten bindings for JavaScript callbacks
extern "C"
{
	EMSCRIPTEN_KEEPALIVE void emscripten_websocket_on_open()
	{
		OnWebSocketOpen();
	}
	EMSCRIPTEN_KEEPALIVE void emscripten_websocket_on_error()
	{
		OnWebSocketError();
	}
	EMSCRIPTEN_KEEPALIVE void emscripten_websocket_on_close()
	{
		OnWebSocketClose();
	}
	EMSCRIPTEN_KEEPALIVE void emscripten_websocket_on_message_js(uintptr_t dataPtr, size_t length)
	{
		if (GSocketInstance && dataPtr && length > 0)
		{
			GSocketInstance->HandleReceive(reinterpret_cast<char const*>(dataPtr), length);
		}
	}
}