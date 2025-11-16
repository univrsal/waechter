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
	std::shared_ptr<WSystemItem> Root = std::make_shared<WSystemItem>();

	ETrafficUnit Unit = TU_MiBps;

	std::unordered_map<WTrafficItemId, std::shared_ptr<ITrafficItem>> TrafficItems;
	std::unordered_set<WTrafficItemId>                                MarkedForRemovalItems;

	std::optional<WEndpoint>    SelectedTupleEndpoint{};
	WTrafficItemId              SelectedItemId{ std::numeric_limits<WTrafficItemId>::max() };
	ETrafficItemType            SelectedItemType{ TI_System };
	std::weak_ptr<ITrafficItem> SelectedItem{};

	void RemoveTrafficItem(WTrafficItemId TrafficItemId);

	bool RenderItem(std::string const& Name, std::shared_ptr<ITrafficItem> Item, ImGuiTreeNodeFlags NodeFlags,
		ETrafficItemType Type, WEndpoint const* ParentItem = nullptr);

	std::mutex DataMutex;

public:
	WTrafficTree() = default;
	~WTrafficTree() = default;

	void LoadFromBuffer(WBuffer const& Buffer);
	void UpdateFromBuffer(WBuffer const& Buffer);
	void Draw(ImGuiID MainID);

	template <class T>
	std::shared_ptr<T> const GetSeletedTrafficItem()
	{
		if (auto const Ptr = SelectedItem.lock())
		{
			return std::dynamic_pointer_cast<T>(Ptr);
		}
		return {};
	}

	std::optional<WEndpoint> GetSelectedTupleEndpoint() const { return SelectedTupleEndpoint; }

	ETrafficItemType GetSelectedItemType() const { return SelectedItemType; }
};
