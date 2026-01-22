/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "DaemonUnixSocket.hpp"

#include <filesystem>

#include "tracy/Tracy.hpp"
#include "spdlog/spdlog.h"

#include "ClientUnixSocket.hpp"
#include "Filesystem.hpp"
#include "DaemonClient.hpp"
#include "DaemonConfig.hpp"

void WDaemonUnixSocket::Stop()
{
	Running = false;
	Socket->Close();
	if (ListenThread.joinable())
	{
		ListenThread.join();
	}
}

bool WDaemonUnixSocket::StartListenThread()
{
	if (std::filesystem::exists(SocketPath))
	{
		unlink(SocketPath.c_str());
	}

	if (!Socket->BindAndListen())
	{
		return false;
	}

	auto const& Cfg = WDaemonConfig::GetInstance();
	if (!WFilesystem::SetSocketOwnerAndPermsByName(SocketPath, Cfg.DaemonUser, Cfg.DaemonGroup, Cfg.DaemonSocketMode))
	{
		return false;
	}
	Running = true;
	ListenThread = std::thread(&WDaemonUnixSocket::ListenThreadFunction, this);
	return true;
}

void WDaemonUnixSocket::ListenThreadFunction() const
{
	tracy::SetThreadName("DaemonSocket");
	WBuffer Buffer;
	while (Running)
	{
		if (auto ClientSocket = Socket->Accept(500))
		{
			spdlog::info("Client connected");
			auto NewSocket = std::make_shared<WClientUnixSocket>(ClientSocket);
			auto NewClient = std::make_shared<WDaemonClient>(NewSocket);
			OnNewConnection(NewClient);
		}
	}
}

WDaemonUnixSocket::WDaemonUnixSocket()
{
	SocketPath = WDaemonConfig::GetInstance().DaemonSocketPath;
	Socket = std::make_unique<WServerSocket>(SocketPath);
}
