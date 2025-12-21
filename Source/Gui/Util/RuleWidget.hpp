/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "EBPFCommon.h"
#include "Data/Rule.hpp"

#include <memory>

struct ITrafficItem;

struct WRenderItemArgs;

class WRuleWidget
{
	WTrafficItemRules EmptyDummyRules{};

public:
	void Draw(WRenderItemArgs const& Args, bool bSelected = false);
};
