//
// Created by usr on 22/11/2025.
//

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
