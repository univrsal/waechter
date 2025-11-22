//
// Created by usr on 21/11/2025.
//

#pragma once
#include "EBPFCommon.h"
#include "TrafficItem.hpp"

struct WRuleUpdate
{
	WNetworkItemRules   Rules{};
	WTrafficItemId TrafficItemId{ 0 };

	template <class Archive>
	void serialize(Archive& Ar)
	{
		Ar(Rules);
		Ar(TrafficItemId);
	}
};