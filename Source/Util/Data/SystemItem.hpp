//
// Created by usr on 30/10/2025.
//

#pragma once

#include <string>

#include "TrafficItem.hpp"

class WSystemItem : public ITrafficItem
{
	std::string HostName{ "System" };

public:
	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(ItemId, HostName);
	}
};