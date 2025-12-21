/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "Data/Rule.hpp"
#include "TrafficItem.hpp"

struct WRuleUpdate
{
	WTrafficItemRules Rules{};
	WTrafficItemId    TrafficItemId{ 0 };
	WTrafficItemId    ParentAppId{ 0 }; // Only for socket/process updates

	template <class Archive>
	void serialize(Archive& Ar)
	{
		Ar(Rules);
		Ar(TrafficItemId);
		Ar(ParentAppId);
	}
};