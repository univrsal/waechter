//
// Created by usr on 30/10/2025.
//

#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <ranges>
#include <cassert>

#include "TrafficItem.hpp"
#include "ApplicationItem.hpp"

struct WSystemItem : ITrafficItem
{
	std::string HostName{ "System" };

	std::unordered_map<std::string, std::shared_ptr<WApplicationItem>> Applications;

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(ItemId, DownloadSpeed, UploadSpeed, HostName, TotalDownloadBytes, TotalUploadBytes, Applications);
	}

	[[nodiscard]] ETrafficItemType GetType() const override { return TI_System; }

	bool NoChildren() override
	{
		if (Applications.empty())
		{
			return true;
		}

		for (auto const& App : Applications | std::views::values)
		{
			assert(App);
			if (App && !App->NoChildren())
			{
				return false;
			}
		}
		return true;
	}

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