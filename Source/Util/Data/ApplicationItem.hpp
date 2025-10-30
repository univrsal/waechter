//
// Created by usr on 30/10/2025.
//

#pragma once
#include "TrafficItem.hpp"

#include <string>

class WApplicationItem : public ITrafficItem
{
	std::string ApplicationName;
	std::string ApplicationPath;

public:
	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(ItemId, ApplicationName, ApplicationPath);
	}
};