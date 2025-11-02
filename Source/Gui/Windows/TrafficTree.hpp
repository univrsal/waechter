//
// Created by usr on 28/10/2025.
//

#pragma once
#include "Format.hpp"

#include <imgui.h>
#include <unordered_set>

#include "Buffer.hpp"
#include "Data/SystemItem.hpp"

class WTrafficTree
{
	WSystemItem Root{};

	ETrafficUnit Unit = TU_MiBps;

	std::unordered_map<WTrafficItemId, ITrafficItem*> TrafficItems;
	std::unordered_set<WTrafficItemId>                MarkedForRemovalItems;
	WTrafficItemId                                    SelectedItemId = std::numeric_limits<WTrafficItemId>::max();

	void RemoveTrafficItem(WTrafficItemId TrafficItemId);

	bool RenderItem(std::string const& Name, const ITrafficItem* Item, ImGuiTreeNodeFlags NodeFlags);

public:
	WTrafficTree() = default;

	void LoadFromBuffer(WBuffer const& Buffer);
	void UpdateFromBuffer(WBuffer const& Buffer);
	void Draw(ImGuiID MainID);
};
