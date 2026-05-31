/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <mutex>

#include "Data/Stats.hpp"

class WStatWindow
{
	std::mutex     DataMutex;
	WStatsRequest  Request{};
	WStatsResponse Response{};
	std::string    Title;
	bool           bOpen{ true };

public:
	explicit WStatWindow(WStatsRequest const& InRequest);
	void Draw();

	void SetResponse(WStatsResponse const& InResponse)
	{
		std::scoped_lock Lock(DataMutex);
		Response = InResponse;
	}

	[[nodiscard]] uint32_t GetRequestId() const { return Request.RequestId; }
};
