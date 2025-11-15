//
// Created by usr on 15/11/2025.
//

#pragma once
#include <memory>
#include <unordered_map>

#include "TrafficCounter.hpp"
#include "Data/ApplicationItem.hpp"
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
};
