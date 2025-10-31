//
// Created by usr on 28/10/2025.
//

#pragma once
#include "Format.hpp"

#include "Buffer.hpp"
#include "Types.hpp"
#include "Data/SystemItem.hpp"

class WTrafficTree
{
	WSystemItem Root{};

	ETrafficUnit Unit = TU_MiBps;

	std::unordered_map<WTrafficItemId, ITrafficItem*> TrafficItems;

	void RemoveTrafficItem(WTrafficItemId TrafficItemId);

public:
	WTrafficTree() = default;

	void LoadFromBuffer(WBuffer const& Buffer);
	void UpdateFromBuffer(WBuffer const& Buffer);
	void Draw();
};
