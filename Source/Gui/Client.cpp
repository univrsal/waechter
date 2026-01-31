/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "Client.hpp"

#include <cstdint>

#include "spdlog/spdlog.h"

#include "AppIconAtlas.hpp"
#include "Messages.hpp"
#if !_WIN32 && !__EMSCRIPTEN__
	#include "Communication/UnixSocketSource.hpp"
#endif
#if WAECHTER_WITH_WEBSOCKETCLIENT
	#include "Communication/WebSocketSource.hpp"
#endif
#if EMSCRIPTEN
	#include "Communication/EmscriptenSocketSource.hpp"
#endif
#include "Data/Protocol.hpp"
#include "Util/Settings.hpp"
#include "Windows/GlfwWindow.hpp"

void WClient::OnDataReceived(WBuffer& Buf)
{
	TrafficCounter.PushIncomingTraffic(Buf.GetWritePos());
	TrafficCounter.Refresh();
	DaemonToClientTrafficRate = TrafficCounter.TrafficItem->DownloadSpeed;

	// Dispatch based on 1-byte message type at start of payload
	auto Type = ReadMessageTypeFromBuffer(Buf);
	if (Type == MT_Invalid)
	{
		spdlog::error("Received invalid message type from server (framed)");
		return;
	}
	switch (Type)
	{
		case MT_Handshake:
			HandleHandshake(Buf);
			break;
		case MT_TrafficTree:
			TrafficTree->LoadFromBuffer(Buf);
			break;
		case MT_TrafficTreeUpdate:
			TrafficTree->UpdateFromBuffer(Buf);
			break;
		case MT_AppIconAtlasData:
			WAppIconAtlas::GetInstance().FromAtlasData(Buf);
			break;
		case MT_ResolvedAddresses:
			TrafficTree->SetResolvedAddresses(Buf);
			break;
		case MT_ConnectionHistory:
			WMainWindow::Get().GetConnectionHistoryWindow().Initialize(Buf);
			break;
		case MT_ConnectionHistoryUpdate:
			WMainWindow::Get().GetConnectionHistoryWindow().HandleUpdate(Buf);
			break;
		case MT_MemoryStats:
			WMainWindow::Get().GetMemoryUsageWindow().HandleUpdate(Buf);
			break;
		default:
			spdlog::warn("Received unknown message type from server: {}", static_cast<int>(Type));
			break;
	}
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
	Stop();
#if WAECHTER_WITH_WEBSOCKETCLIENT || EMSCRIPTEN
	if (WSettings::GetInstance().SocketPath.starts_with("ws://")
		|| WSettings::GetInstance().SocketPath.starts_with("wss://"))
	{
	#if EMSCRIPTEN
		DaemonSocket = std::make_shared<WEmscriptenSocketSource>(
			WSettings::GetInstance().SocketPath, WSettings::GetInstance().WebSocketAuthToken);
	#else
		DaemonSocket = std::make_shared<WWebSocketSource>(
			WSettings::GetInstance().SocketPath, WSettings::GetInstance().WebSocketAuthToken);
	#endif
		DaemonSocket->GetDataSignal().connect([this](WBuffer& Buf) { OnDataReceived(Buf); });
	}
	else
#endif
	if (WSettings::GetInstance().SocketPath.starts_with('/'))
	{
#if _WIN32 || __EMSCRIPTEN__
		spdlog::error("Unix socket is not supported on Windows/emscripten");
#else
		DaemonSocket = std::make_shared<WUnixSocketSource>();
		DaemonSocket->GetDataSignal().connect([this](WBuffer& Buf) { OnDataReceived(Buf); });
#endif
	}
	else
	{
		spdlog::error("Invalid socket path: {}", WSettings::GetInstance().SocketPath);
	}
	if (DaemonSocket)
	{
		DaemonSocket->Start();
	}
}

WClient::WClient()
{
	TrafficTree = std::make_shared<WTrafficTree>();
}