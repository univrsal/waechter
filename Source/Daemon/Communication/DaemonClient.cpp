/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "DaemonClient.hpp"

#include <spdlog/spdlog.h>

#include "Messages.hpp"
#include "DaemonSocket.hpp"
#include "Rules/RuleManager.hpp"

void WDaemonClient::ListenThreadFunction()
{
	WBuffer RecvBuf;
	while (Running)
	{
		bool bHaveFrame = ClientSocket->ReceiveFramed(RecvBuf);

		if (!ClientSocket->IsConnected())
		{
			Running = false;
			break;
		}

		if (!bHaveFrame)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			continue;
		}

		// Extract the full message
		auto Type = ReadMessageTypeFromBuffer(RecvBuf);
		if (Type == MT_Invalid)
		{
			spdlog::warn("Received invalid message type from daemon client");
			continue;
		}
		spdlog::info("Received message: {}", static_cast<int>(Type));

		switch (Type)
		{
			case MT_RuleUpdate:
				WRuleManager::GetInstance().HandleRuleChange(RecvBuf);
				break;
			default:
				break;
		}
		RecvBuf.Compact();
	}
	ClientSocket->Close();
	spdlog::info("Client disconnected");
}

void WDaemonClient::StartListenThread()
{
	Running = true;
	ListenThread = std::thread(&WDaemonClient::ListenThreadFunction, this);
}
