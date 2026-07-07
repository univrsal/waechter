/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <optional>

#include "Data/Rule.hpp"
#include "Data/TrafficItem.hpp"

struct ITrafficItem;

struct WRenderItemArgs;

class WRuleWidget
{
	WTrafficItemRules EmptyDummyRules{};
	std::string       PendingPopupName{};
	std::optional<WTrafficItemId> PendingPopupItemId{};
	bool              bOpenPendingPopup{};

	void DrawWarningPopup(WRenderItemArgs const& Args);

public:
	void Draw(WRenderItemArgs const& Args, bool bSelected = false);
};
