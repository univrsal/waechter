//
// Created by usr on 30/10/2025.
//

#pragma once
#include <string>
#include <unordered_map>
#include <memory>

#include "TrafficItem.hpp"
#include "ProcessItem.hpp"

class WApplicationItem : public ITrafficItem
{
public:
	std::string ApplicationName;
	std::string ApplicationPath;

	std::unordered_map<WProcessId, std::shared_ptr<WProcessItem>> Processes;

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(ItemId, DownloadSpeed, UploadSpeed, TotalDownloadBytes, TotalUploadBytes, ApplicationName, ApplicationPath, Processes);
	}
};