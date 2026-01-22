/*
 * Copyright (c) 2026-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ConnectionHistoryWindow.hpp"

#include "cereal/types/unordered_map.hpp"
#include "cereal/types/vector.hpp"
// ReSharper disable once CppUnusedIncludeDirective
#include "cereal/types/array.hpp"

#include "Client.hpp"
#include "TrafficTree.hpp"
#include "Format.hpp"
#include "Data/ConnectionHistoryUpdate.hpp"

void WConnectionHistoryWindow::PushNewItem(WNewConnectionHistoryEntry const& NewEntry)
{
	WConnectionHistoryListItem ListItem;
	ListItem.DataIn = NewEntry.DataIn;
	ListItem.DataOut = NewEntry.DataOut;
	ListItem.RemoteEndpoint = NewEntry.RemoteEndpoint;
	ListItem.StartTime = WTimeFormat::FormatUnixTime(NewEntry.StartTime);
	ListItem.ConnectionId = NewEntry.ConnectionId;
	if (NewEntry.EndTime > 0)
	{
		ListItem.EndTime = WTimeFormat::FormatUnixTime(NewEntry.EndTime);
	}
	else
	{
		ListItem.EndTime = "n/a";
	}

	if (NewEntry.AppId > 0)
	{
		auto Item = WClient::GetInstance().GetTrafficTree()->GetItemFromId(NewEntry.AppId);

		if (Item)
		{
			if (Item->GetType() == TI_Application)
			{
				ListItem.App = std::static_pointer_cast<WApplicationItem>(Item);
			}
#if WDEBUG
			else
			{
				spdlog::warn("Connection history entry received for non-application item ID {}", NewEntry.AppId);
			}
#endif
		}
	}

	HistoryItems.push_back(ListItem);
	if (HistoryItems.size() > kMaxHistorySize)
	{
		HistoryItems.pop_front();
	}
}

void WConnectionHistoryWindow::Draw()
{
	if (ImGui::Begin("Connection History", nullptr, ImGuiWindowFlags_NoCollapse))
	{
		std::scoped_lock Lock(Mutex);

		if (ImGui::BeginTable("ConnectionHistoryTable", 7,
				ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
		{
			ImGui::TableSetupColumn("App Name", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("IP", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Port", ImGuiTableColumnFlags_WidthFixed, 80.0f);
			ImGui::TableSetupColumn("Data In", ImGuiTableColumnFlags_WidthFixed, 100.0f);
			ImGui::TableSetupColumn("Data Out", ImGuiTableColumnFlags_WidthFixed, 100.0f);
			ImGui::TableSetupColumn("Start Time", ImGuiTableColumnFlags_WidthFixed, 150.0f);
			ImGui::TableSetupColumn("End Time", ImGuiTableColumnFlags_WidthFixed, 150.0f);
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();

			for (auto const& Item : HistoryItems)
			{
				if (Item.DataIn == 0 && Item.DataOut == 0)
				{
					continue;
				}

				if (Item.RemoteEndpoint.Address.IsZero() || Item.RemoteEndpoint.Address.IsBroadcast()
					|| Item.RemoteEndpoint.Port == 0)
				{
					continue;
				}

				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				ImGui::Text("%s", Item.App ? Item.App->ApplicationName.c_str() : "");

				ImGui::TableNextColumn();
				ImGui::Text("%s", Item.RemoteEndpoint.Address.ToString().c_str());

				ImGui::TableNextColumn();
				ImGui::Text("%u", Item.RemoteEndpoint.Port);

				ImGui::TableNextColumn();
				ImGui::Text("%s", WStorageFormat::AutoFormat(Item.DataIn).c_str());

				ImGui::TableNextColumn();
				ImGui::Text("%s", WStorageFormat::AutoFormat(Item.DataOut).c_str());

				ImGui::TableNextColumn();
				ImGui::Text("%s", Item.StartTime.c_str());

				ImGui::TableNextColumn();
				ImGui::Text("%s", Item.EndTime.c_str());
			}

			ImGui::EndTable();
		}
	}
	ImGui::End();
}

void WConnectionHistoryWindow::Initialize(WBuffer& Update)
{
	std::scoped_lock Lock(Mutex);
	HistoryItems.clear();

	WConnectionHistoryUpdate HistoryUpdate{};
	if (WClient::ReadMessage(Update, HistoryUpdate) == false)
	{
		spdlog::error("Failed to read connection history update");
		return;
	}

	for (auto const& NewEntry : HistoryUpdate.NewEntries)
	{
		PushNewItem(NewEntry);
	}
}

void WConnectionHistoryWindow::HandleUpdate(WBuffer& Update)
{
	std::scoped_lock Lock(Mutex);

	WConnectionHistoryUpdate HistoryUpdate{};
	if (WClient::ReadMessage(Update, HistoryUpdate) == false)
	{
		spdlog::error("Failed to read connection history update");
		return;
	}

	for (auto const& NewEntry : HistoryUpdate.NewEntries)
	{
		PushNewItem(NewEntry);
	}

	for (auto& Item : HistoryItems)
	{
		if (auto It = HistoryUpdate.Changes.find(Item.ConnectionId); It != HistoryUpdate.Changes.end())
		{
			auto const& Change = It->second;
			Item.DataIn = Change.NewDataIn;
			Item.DataOut = Change.NewDataOut;
			if (Change.NewEndTime > 0)
			{
				Item.EndTime = WTimeFormat::FormatUnixTime(Change.NewEndTime);
			}
		}
	}
}