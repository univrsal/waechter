/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <unordered_map>

#include "TrafficItem.hpp"
#include "SocketItem.hpp"
#include "Types.hpp"

struct WProcessItem : ITrafficItem
{
	WProcessId ProcessId{};

	std::unordered_map<WSocketCookie, std::shared_ptr<WSocketItem>> Sockets;
	template <class Archive>

	void serialize(Archive& archive)
	{
		archive(ItemId, DownloadSpeed, UploadSpeed, TotalDownloadBytes, TotalUploadBytes, ProcessId, Sockets);
	}

	[[nodiscard]] ETrafficItemType GetType() const override { return TI_Process; }

	bool NoChildren() override { return Sockets.empty(); }

	bool RemoveChild(WTrafficItemId TrafficItemId) override
	{
		for (auto It = Sockets.begin(); It != Sockets.end(); ++It)
		{
			if (It->second->ItemId == TrafficItemId)
			{
				Sockets.erase(It);
				return true;
			}
		}
		return false;
	}
};
