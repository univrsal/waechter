/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "DaemonClient.hpp"

#include "spdlog/spdlog.h"

#include "Messages.hpp"
#include "Data/ResolveData.hpp"
#include "Data/Stats.hpp"
#include "Db/StatsManager.hpp"
#include "Net/Resolver.hpp"
#include "Rules/RuleManager.hpp"

void WDaemonClient::OnDataReceived(WBuffer& RecvBuf)
{
	auto const Type = ReadMessageTypeFromBuffer(RecvBuf);
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
		case MT_ResolveRequest:
			HandleResolveRequest(RecvBuf);
			break;
		case MT_StatsRequest:
			HandleStatsRequest(RecvBuf);
			break;
		default:
			break;
	}
}

void WDaemonClient::HandleStatsRequest(WBuffer const& Buf) const
{
	spdlog::info("Received stats request from client");
	WStatsRequest Request{};
	if (!DeserializeMessage(Buf, Request))
	{
		spdlog::error("Failed to deserialize stats request");
		return;
	}

	WStatsManager::GetInstance().RequestStats(Request).Then([this, Request](WStatsResponse const& Response) {
		if (!ClientSocket->IsConnected())
		{
			spdlog::warn(
				"Client disconnected before stats response could be sent for request id {}", Request.RequestId);
			return;
		}
		ClientSocket->SendFramed(MakeMessage(MT_StatsResponse, Response));
	});
}

void WDaemonClient::HandleResolveRequest(WBuffer const& Buf)
{
	WResolveRequest Request{};
	if (!DeserializeMessage(Buf, Request))
	{
		spdlog::error("Failed to deserialize resolve request");
		return;
	}
	WResolver::GetInstance()
		.Resolve(Request.AddressToResolve)
		.Then([Request, Socket = ClientSocket](std::string const& Hostname) {
			if (!Socket->IsConnected())
			{
				spdlog::warn("Client disconnected before resolve response could be sent for {}",
					Request.AddressToResolve.ToString());
				return;
			}
			spdlog::debug(
				"Finished resolve request for address: {} -> {}", Request.AddressToResolve.ToString(), Hostname);
			WResolveResponse Response{};
			Response.AddressToResolve = Request.AddressToResolve;
			Response.ResolveResult = Hostname;
			Socket->SendFramed(MakeMessage(MT_ResolveResponse, Response));
		});
}
