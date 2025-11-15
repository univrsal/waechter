//
// Created by usr on 28/10/2025.
//

#pragma once
#include <imgui.h>
#include <unordered_set>

#include "Format.hpp"
#include "Buffer.hpp"
#include "Data/SystemItem.hpp"

class WTrafficTree
{
	WSystemItem Root{};

	ETrafficUnit Unit = TU_MiBps;

	std::unordered_map<WTrafficItemId, ITrafficItem*> TrafficItems;
	std::unordered_set<WTrafficItemId>                MarkedForRemovalItems;

	WTrafficItemId      SelectedItemId = std::numeric_limits<WTrafficItemId>::max();
	ETrafficItemType    SelectedItemType = TI_System;
	ITrafficItem const* SelectedItem = nullptr;

	void RemoveTrafficItem(WTrafficItemId TrafficItemId);

	bool RenderItem(
		std::string const& Name, ITrafficItem const* Item, ImGuiTreeNodeFlags NodeFlags, ETrafficItemType Type);

public:
	WTrafficTree() = default;
	~WTrafficTree() = default;

	void LoadFromBuffer(WBuffer const& Buffer);
	void UpdateFromBuffer(WBuffer const& Buffer);
	void Draw(ImGuiID MainID);

	template <class T>
	T const* GetSeletedTrafficItem()
	{
		if (SelectedItem != nullptr)
		{
			return static_cast<T const*>(SelectedItem);
		}
		return nullptr;
	}

	ETrafficItemType GetSelectedItemType() const { return SelectedItemType; }
};
