/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "DaemonClient.hpp"

#include <spdlog/spdlog.h>

#include "Messages.hpp"
#include "Rules/RuleManager.hpp"

void WDaemonClient::OnDataReceived(WBuffer& RecvBuf)
{

	// Extract the full message
	auto Type = ReadMessageTypeFromBuffer(RecvBuf);
	if (Type == MT_Invalid)
	{
		spdlog::warn("Received invalid message type from daemon client");
		return;
	}

	switch (Type)
	{
		case MT_RuleUpdate:
			WRuleManager::GetInstance().HandleRuleChange(RecvBuf);
			break;
		default:
			break;
	}
}
