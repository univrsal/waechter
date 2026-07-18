/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <memory>
#include <unordered_map>
#include <functional>

#include "spdlog/spdlog.h"

#include "TrafficCounter.hpp"
#include "Data/ApplicationItem.hpp"
#include "Data/FilterItem.hpp"
#include "EBPFCommon.h"

struct WAppCounter : TTrafficCounter<WApplicationItem>
{
	explicit WAppCounter(std::shared_ptr<WApplicationItem> const& Item) : TTrafficCounter(Item) {}
};

struct WProcessCounter : TTrafficCounter<WProcessItem>
{
	explicit WProcessCounter(std::shared_ptr<WProcessItem> const& Item, std::shared_ptr<WAppCounter> const& ParentApp_)
		: TTrafficCounter(Item), ParentApp(ParentApp_)
	{
	}
	std::shared_ptr<WAppCounter> ParentApp;
};

struct WTupleCounter;

struct WSocketCounter : TTrafficCounter<WSocketItem>
{
	explicit WSocketCounter(
		std::shared_ptr<WSocketItem> const& Item, std::shared_ptr<WProcessCounter> const& ParentProcess_)
		: TTrafficCounter(Item), ParentProcess(ParentProcess_)
	{
	}

	void ProcessSocketEvent(WSocketEvent const& Event) const;

	std::shared_ptr<WProcessCounter>                              ParentProcess;
	std::unordered_map<WEndpoint, std::shared_ptr<WTupleCounter>> UDPPerConnectionCounters{};
};

struct WTupleCounter : TTrafficCounter<WTupleItem>
{
	explicit WTupleCounter(
		std::shared_ptr<WTupleItem> const& Item, std::shared_ptr<WSocketCounter> const& ParentSocket_)
		: TTrafficCounter(Item), ParentSocket(ParentSocket_)
	{
	}

	std::shared_ptr<WSocketCounter> ParentSocket;

	void Refresh() override
	{
		TTrafficCounter::Refresh();
		// If a UDP socket has not sent/received data on a connection for sixty seconds,
		// we'll treat it as dead. In the worst case it'll be re-added once traffic
		// is detected for it again
		if (State != CS_PendingRemoval && InactiveCounter >= 60)
		{
			spdlog::debug("Marking stale udp counter {} for removal", TrafficItem->ItemId);
			MarkForRemoval();
		}
	}
};

struct WFilterCounter : TTrafficCounter<WFilterItem>
{
	std::function<bool(WTrafficItemId const&, WEndpoint const*, WEndpoint const*)> FilterFunction;

	explicit WFilterCounter(std::shared_ptr<WFilterItem> const& Item) : TTrafficCounter(Item) {}
};
