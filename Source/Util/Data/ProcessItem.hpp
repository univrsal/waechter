//
// Created by usr on 30/10/2025.
//

#pragma once

#pragma once
#include "TrafficItem.hpp"
#include "Types.hpp"

class WProcessItem : public ITrafficItem
{
	WProcessId ProcessId{};

public:
	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(ItemId, ProcessId);
	}
};
