/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <ranges>
#include <cassert>

#include "TrafficItem.hpp"
#include "ApplicationItem.hpp"
#include "FilterItem.hpp"

struct WSystemItem : ITrafficItem
{
	std::string HostName{ "System" };

	std::vector<std::shared_ptr<WFilterItem>> Filters;

	std::unordered_map<std::string, std::shared_ptr<WApplicationItem>> Applications;

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(
			ItemId, DownloadSpeed, UploadSpeed, HostName, TotalDownloadBytes, TotalUploadBytes, Applications, Filters);
	}

	[[nodiscard]] ETrafficItemType GetType() const override { return TI_System; }

	bool HasChildren() override { return !Applications.empty(); }

	bool RemoveChild(WTrafficItemId TrafficItemId) override
	{
		for (auto It = Applications.begin(); It != Applications.end(); ++It)
		{
			assert(It->second);
			if (It->second && It->second->ItemId == TrafficItemId)
			{
				Applications.erase(It);
				return true;
			}
		}
		return false;
	}
};