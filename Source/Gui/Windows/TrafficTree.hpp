/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <unordered_set>
#include <mutex>

#include "imgui.h"

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

struct WTreeNode
{
	std::shared_ptr<ITrafficItem>           Item{};
	std::vector<std::shared_ptr<WTreeNode>> Children{};
	WEndpoint                               TupleEndpoint{};
};

class WTrafficTree
{
	WRuleWidget                  RuleWidget{};
	std::shared_ptr<WSystemItem> Root = std::make_shared<WSystemItem>();

	std::unordered_map<WTrafficItemId, std::shared_ptr<ITrafficItem>> TrafficItems;
	std::unordered_set<WTrafficItemId>                                MarkedForRemovalItems;

	// Used for display, uses vectors instead of maps so we can display things sorted,
	// has to be rebuilt whenever the tree changes (item removed/added, column sorted)
	std::shared_ptr<WTreeNode> TreeRoot{};

	std::unordered_map<WIPAddress, std::string> ResolvedAddresses{};

	std::optional<WEndpoint>    SelectedTupleEndpoint{};
	WTrafficItemId              SelectedItemId{ std::numeric_limits<WTrafficItemId>::max() };
	ETrafficItemType            SelectedItemType{ TI_System };
	std::weak_ptr<ITrafficItem> SelectedItem{};

	char SearchBuffer[256] = "";
	bool bRequireTreeSorting{};

	void RemoveTrafficItem(WTrafficItemId TrafficItemId);

	bool RenderItem(WRenderItemArgs const& Args);

	std::mutex DataMutex;

	void SortTree(ImGuiTableSortSpecs const* Specs);

public:
	WTrafficTree() = default;
	~WTrafficTree() = default;

	void LoadFromBuffer(WBuffer const& Buffer);
	void UpdateFromBuffer(WBuffer const& Buffer);
	void Draw(ImGuiID MainID);

	void HandleResolveResponse(WBuffer const& Buffer);

	std::string const& ResolveAddress(WIPAddress const& Address);

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

	void Clear()
	{
		std::lock_guard Lock(DataMutex);
		TrafficItems.clear();
		ResolvedAddresses.clear();
		SelectedItemId = std::numeric_limits<WTrafficItemId>::max();
		SelectedItem = {};
		Root = std::make_shared<WSystemItem>();
	}
};
