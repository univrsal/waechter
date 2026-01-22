/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "Data/Rule.hpp"

struct ITrafficItem;

struct WRenderItemArgs;

class WRuleWidget
{
	WTrafficItemRules EmptyDummyRules{};

public:
	void Draw(WRenderItemArgs const& Args, bool bSelected = false);
};
