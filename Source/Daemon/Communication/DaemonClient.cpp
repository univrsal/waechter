/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "DaemonClient.hpp"

#include "spdlog/spdlog.h"

#include "Messages.hpp"
#include "Data/IP2Asn.hpp"
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
			WRuleManager::GetInstance().HandleRuleChange(RecvBuf, this);
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
		case MT_IPLookupRequest:
			HandleIPLookupRequest(RecvBuf);
			break;
		case MT_UpdateIP2AsnDb:
			WIP2Asn::GetInstance().UpdateDatabase();
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

void WDaemonClient::HandleIPLookupRequest(WBuffer const& Buf)
{
	WIPLookupRequest Request{};
	if (!DeserializeMessage(Buf, Request))
	{
		spdlog::error("Failed to deserialize IP lookup request");
		return;
	}
	spdlog::debug("Received lookup request for {}", Request.AddressToLookup.ToString());

	WIP2Asn::GetInstance()
		.Lookup(Request.AddressToLookup)
		.Then([Request, Socket = ClientSocket](std::optional<WIP2AsnLookupResult> const& Result) {
			if (!Socket->IsConnected())
			{
				spdlog::warn("Client disconnected before IP lookup response could be sent for {}",
					Request.AddressToLookup.ToString());
				return;
			}
			if (!Result.has_value())
			{
				spdlog::warn("IP lookup failed for address: {}", Request.AddressToLookup.ToString());
				return;
			}
			spdlog::debug("Finished IP lookup request for address: {} -> ASN: {}, Country: {}, Organization: {}",
				Request.AddressToLookup.ToString(), Result->ASN, Result->Country, Result->Organization);
			Socket->SendFramed(MakeMessage(MT_IPLookupResponse, Result.value()));
		});
}