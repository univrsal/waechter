/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <ranges>

#include "TrafficItem.hpp"
#include "ProcessItem.hpp"

class WApplicationItem : public ITrafficItem
{
public:
	std::string ApplicationName;        // ssh
	std::string ApplicationPath;        // /usr/bin/ssh
	std::string ApplicationCommandLine; // /usr/bin/ssh user@host

	std::unordered_map<WProcessId, std::shared_ptr<WProcessItem>> Processes;

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(ItemId, DownloadSpeed, UploadSpeed, TotalDownloadBytes, TotalUploadBytes, ApplicationName, ApplicationPath, ApplicationCommandLine, Processes);
	}

	[[nodiscard]] ETrafficItemType GetType() const override
	{
		return TI_Application;
	}

	bool NoChildren() override
	{
		if (Processes.empty())
		{
			return true;
		}

		for (auto const& Process : Processes | std::views::values)
		{
			if (!Process->NoChildren())
			{
				return false;
			}
		}
		return true;
	}

	bool RemoveChild(WTrafficItemId TrafficItemId) override
	{
		for (auto It = Processes.begin(); It != Processes.end(); ++It)
		{
			if (It->second->ItemId == TrafficItemId)
			{
				Processes.erase(It);
				return true;
			}
		}
		return false;
	}
};