/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <deque>
#include <mutex>
#include <memory>

#include "Buffer.hpp"
#include "IPAddress.hpp"
#include "Types.hpp"

struct WApplicationItem;
struct WNewConnectionHistoryEntry;

struct WConnectionHistoryListItem
{
	std::shared_ptr<WApplicationItem> App;

	WEndpoint   RemoteEndpoint{};
	WBytes      DataIn{ 0 };
	WBytes      DataOut{ 0 };
	std::string StartTime{ 0 };
	std::string EndTime{ 0 };
};

class WConnectionHistoryWindow
{
	std::mutex Mutex;

	std::deque<WConnectionHistoryListItem> HistoryItems;

	void PushNewItem(WNewConnectionHistoryEntry const& Item);

public:
	void Draw();

	void Initialize(WBuffer& Update);

	void HandleUpdate(WBuffer& Update);
};
