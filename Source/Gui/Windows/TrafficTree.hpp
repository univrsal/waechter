/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <imgui.h>
#include <unordered_set>

#include "Format.hpp"
#include "Buffer.hpp"
#include "Data/SystemItem.hpp"
#include "Util/RuleWidget.hpp"

struct WRenderItemArgs
{
	std::string                   Name{};
	std::shared_ptr<ITrafficItem> Item{};
	ImGuiTreeNodeFlags            NodeFlags{};
	bool                          bMarkedForRemoval{};
	WEndpoint const*              TupleEndpoint{};
	std::shared_ptr<ITrafficItem> ParentApp{};
};

class WTrafficTree
{
	WRuleWidget                  RuleWidget{};
	std::shared_ptr<WSystemItem> Root = std::make_shared<WSystemItem>();

	ETrafficUnit Unit = TU_Auto;

	std::unordered_map<WTrafficItemId, std::shared_ptr<ITrafficItem>> TrafficItems;
	std::unordered_set<WTrafficItemId>                                MarkedForRemovalItems;

	std::unordered_map<WIPAddress, std::string> ResolvedAddresses{};

	std::optional<WEndpoint>    SelectedTupleEndpoint{};
	WTrafficItemId              SelectedItemId{ std::numeric_limits<WTrafficItemId>::max() };
	ETrafficItemType            SelectedItemType{ TI_System };
	std::weak_ptr<ITrafficItem> SelectedItem{};

	char SearchBuffer[256] = "";

	void RemoveTrafficItem(WTrafficItemId TrafficItemId);

	bool RenderItem(WRenderItemArgs const& Args);

	std::mutex DataMutex;

public:
	WTrafficTree() = default;
	~WTrafficTree() = default;

	void LoadFromBuffer(WBuffer const& Buffer);
	void UpdateFromBuffer(WBuffer const& Buffer);
	void Draw(ImGuiID MainID);

	void SetResolvedAddresses(WBuffer const& Buffer);

	std::unordered_map<WIPAddress, std::string> const& GetResolvedAddresses() const { return ResolvedAddresses; }

	template <class T>
	std::shared_ptr<T> GetSeletedTrafficItem()
	{
		if (auto const Ptr = SelectedItem.lock())
		{
			return std::dynamic_pointer_cast<T>(Ptr);
		}
		return {};
	}

	std::optional<WEndpoint> GetSelectedTupleEndpoint() const { return SelectedTupleEndpoint; }

	ETrafficItemType GetSelectedItemType() const { return SelectedItemType; }

	std::shared_ptr<ITrafficItem> GetItemFromId(WTrafficItemId ItemId)
	{
		std::lock_guard Lock(DataMutex);
		if (auto const It = TrafficItems.find(ItemId); It != TrafficItems.end())
		{
			return It->second;
		}
		return {};
	}
};
