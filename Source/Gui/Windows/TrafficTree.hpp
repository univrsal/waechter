//
// Created by usr on 28/10/2025.
//

#pragma once
#include "Format.hpp"

#include <string>
#include <vector>
#include <memory>

#include "Types.hpp"

#include <unordered_map>

struct WTrafficTreeNode
{
	std::string     Name{};
	std::string     Tooltip{};
	WBytesPerSecond Upload{};
	WBytesPerSecond Download{};
	WBytesPerSecond UploadLimit{};
	WBytesPerSecond DownloadLimit{};
	bool            bPendingRemoval = false;

	std::vector<std::shared_ptr<WTrafficTreeNode>> Children{};
};

class WTrafficTree
{
	WTrafficTreeNode Root{};

	ETrafficUnit Unit = TU_MiBps;

	uint64_t TreeNodeCounter = 0;

	std::unordered_map<uint64_t, std::shared_ptr<WTrafficTreeNode>> Nodes{};

public:
	WTrafficTree() = default;

	void LoadFromJson(std::string const& Json);
	void UpdateFromJson(std::string const& Json);
	void Draw();
};
