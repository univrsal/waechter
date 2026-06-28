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
		case MT_HistoryRequest:
		case MT_StatsRequest:
			WStatsManager::GetInstance()
				.RequestStats(RecvBuf, Type)
				.Then([Socket = ClientSocket](std::string const& Response) {
					if (Response.empty())
					{
						spdlog::warn("Failed to process stats request, response is empty");
					}
					else if (!Socket->IsConnected())
					{
						spdlog::warn("Client disconnected before stats response could be sent");
					}
					else
					{
						Socket->SendFramed(Response);
					}
				});
			break;
		default:
			break;
	}
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