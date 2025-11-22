//
// Created by usr on 22/11/2025.
//

#pragma once
#include "EBPFCommon.h"

#include <memory>

struct ITrafficItem;

class WRuleWidget
{
	WNetworkItemRules EmptyDummyRules{};

public:
	void Draw(std::shared_ptr<ITrafficItem> const& Item, bool bSelected = false);
};
