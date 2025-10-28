//
// Created by usr on 28/10/2025.
//

#pragma once
#include <string>
#include <vector>
#include <memory>

#include "Types.hpp"

struct WTrafficTreeNode
{
	std::string     Name{};
	std::string     Tooltip{};
	WBytesPerSecond Upload{};
	WBytesPerSecond Download{};
	WBytesPerSecond UploadLimit{};
	WBytesPerSecond DownloadLimit{};

	std::vector<std::unique_ptr<WTrafficTreeNode>> Children{};
};

class WTrafficTree
{
	WTrafficTreeNode Root{};

public:
	WTrafficTree() = default;

	void LoadFromJson(std::string const& Json);
	void Draw();
};
