//
// Created by usr on 30/10/2025.
//

#pragma once

#include <string>
#include <unordered_map>
#include <memory>

#include "TrafficItem.hpp"
#include "ApplicationItem.hpp"

class WSystemItem : public ITrafficItem
{
public:
	std::string HostName{ "System" };

	std::unordered_map<std::string, std::shared_ptr<WApplicationItem>> Applications;

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(ItemId, HostName, Applications, DownloadSpeed, UploadSpeed);
	}
};