/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "TrafficTree.hpp"

// ReSharper disable CppUnusedIncludeDirective
#include <imgui.h>
#include <ranges>
#include <algorithm>
#include <spdlog/spdlog.h>
#include <cereal/types/optional.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/archives/binary.hpp>

#include "AppIconAtlas.hpp"
#include "ClientRuleManager.hpp"
#include "Util/ImGuiUtil.hpp"
#include "Format.hpp"
#include "GlfwWindow.hpp"
#include "Messages.hpp"
#include "Data/TrafficTreeUpdate.hpp"
#include "Util/Settings.hpp"
#include "spdlog/fmt/bundled/chrono.h"

// This isn't exactly efficient, we should probably have something
// along the lines of ITrafficItem->Parent->RemoveChild or similar.
void WTrafficTree::RemoveTrafficItem(WTrafficItemId TrafficItemId)
{
	MarkedForRemovalItems.erase(TrafficItemId);
	WClientRuleManager::GetInstance().RemoveRules(TrafficItemId);

	if (Root->RemoveChild(TrafficItemId))
	{
		return;
	}

	for (auto& App : Root->Applications | std::views::values)
	{
		assert(App);
		if (!App)
		{
			continue;
		}

		if (App->RemoveChild(TrafficItemId))
		{
			return;
		}

		for (auto& Proc : App->Processes | std::views::values)
		{
			if (Proc->RemoveChild(TrafficItemId))
			{
				return;
			}
		}
	}
}

inline bool DrawIcon(
	bool& bNodeOpen, std::string const& Name, std::shared_ptr<ITrafficItem> const& Item, ImGuiTreeNodeFlags NodeFlags)
{
	// For applications with icons: use TreeNodeEx with pointer ID and empty label,
	// then draw icon and text inline. This puts icon after the arrow.
	bNodeOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(Item->ItemId)), NodeFlags, "%s", "");

	// Store the clicked state immediately after TreeNodeEx, before drawing icon/text
	bool bClicked = ImGui::IsItemClicked();

	// Reduce spacing between arrow and icon
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, ImGui::GetStyle().ItemSpacing.y));
	ImGui::SameLine();
	if (Item->GetType() == TI_Application)
	{
		std::lock_guard Lock(WAppIconAtlas::GetInstance().GetMutex());
		WAppIconAtlas::GetInstance().DrawIconForApplication(
			std::dynamic_pointer_cast<WApplicationItem>(Item)->ApplicationName, ImVec2{ 16, 16 });
	}
	else if (Item->GetType() == TI_Process)
	{
		WIconAtlas::GetInstance().DrawIcon("process", ImVec2{ 16, 16 });
	}
	else
	{
		WIconAtlas::GetInstance().DrawIcon("computer", ImVec2{ 16, 16 });
	}
	ImGui::SameLine();
	ImGui::TextUnformatted(Name.c_str());
	ImGui::PopStyleVar();

	// Also check if icon or text was clicked
	return bClicked || ImGui::IsItemClicked();
}

bool WTrafficTree::RenderItem(WRenderItemArgs const& Args)
{
	auto Type = Args.Item->GetType();
	auto NodeFlags = Args.NodeFlags;
	NodeFlags |= ImGuiTreeNodeFlags_SpanFullWidth;
	NodeFlags |= ImGuiTreeNodeFlags_OpenOnArrow;
	ImGui::TableSetColumnIndex(0);

	bool bRowSelected = SelectedItemId == Args.Item->ItemId;
	if (bRowSelected)
	{
		ImU32 RowColor = ImGui::GetColorU32(ImGuiCol_Header);
		ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, RowColor);
		ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, RowColor);
	}

	if (Args.bMarkedForRemoval)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{ 1.0f, 0.4f, 0.4f, 1.0f });
	}

	if (SelectedItemId == Args.Item->ItemId)
	{
		NodeFlags |= ImGuiTreeNodeFlags_Selected;
	}

	bool bNodeOpen = false;
	bool bItemClicked = false;

	if (Args.Item->GetType() <= TI_Process)
	{
		bItemClicked = DrawIcon(bNodeOpen, Args.Name, Args.Item, NodeFlags);
	}
	else
	{
		bNodeOpen = ImGui::TreeNodeEx(Args.Name.c_str(), NodeFlags);
		bItemClicked = ImGui::IsItemClicked();
	}

	if (Args.bMarkedForRemoval)
	{
		ImGui::PopStyleColor();
	}

	if (bItemClicked)
	{
		SelectedItemId = Args.Item->ItemId;
		SelectedItem = Args.Item;
		SelectedItemType = Type;
		if (Args.TupleEndpoint)
		{
			SelectedTupleEndpoint = *Args.TupleEndpoint;
		}
		else
		{
			SelectedTupleEndpoint = std::nullopt;
		}
	}

#if WDEBUG
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::Text("ID: %llu", Args.Item->ItemId);
		ImGui::EndTooltip();
	}
#endif

	ImGui::TableSetColumnIndex(1);
	if (Args.Item->DownloadSpeed > 0)
	{
		ImGui::Text("%s",
			WTrafficFormat::Format(Args.Item->DownloadSpeed, WSettings::GetInstance().TrafficTreeUnitSetting).c_str());
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::Text("%s", "0.00 B/s");
		ImGui::PopStyleColor();
	}

	ImGui::TableSetColumnIndex(2);
	if (Args.Item->UploadSpeed > 0)
	{
		ImGui::Text("%s",
			WTrafficFormat::Format(Args.Item->UploadSpeed, WSettings::GetInstance().TrafficTreeUnitSetting).c_str());
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::Text("%s", "0.00 B/s");
		ImGui::PopStyleColor();
	}

	ImGui::TableSetColumnIndex(3);
	ImGui::PushID(static_cast<int>(Args.Item->ItemId));
	RuleWidget.Draw(Args, bRowSelected);
	ImGui::PopID();

	return bNodeOpen;
}

void WTrafficTree::LoadFromBuffer(WBuffer const& Buffer)
{
	std::lock_guard Lock(DataMutex);
	TrafficItems.clear();
	std::stringstream ss;
	ss.write(Buffer.GetData(), static_cast<long int>(Buffer.GetWritePos()));
	{
		ss.seekg(1); // Skip message type
		cereal::BinaryInputArchive iar(ss);
		iar(*Root.get());
	}

	// Remove any NULL entries that may have been deserialized
	for (auto It = Root->Applications.begin(); It != Root->Applications.end();)
	{
		if (!It->second)
		{
			spdlog::warn("Found NULL application '{}' in deserialized data, removing", It->first);
			It = Root->Applications.erase(It);
		}
		else
		{
			++It;
		}
	}

	for (auto const& App : Root->Applications | std::views::values)
	{
		TrafficItems[App->ItemId] = App;
		for (auto const& Proc : App->Processes | std::views::values)
		{
			TrafficItems[Proc->ItemId] = Proc;
			for (auto const& Sock : Proc->Sockets | std::views::values)
			{
				TrafficItems[Sock->ItemId] = Sock;
			}
		}
	}

	TrafficItems[0] = Root;
}

void WTrafficTree::UpdateFromBuffer(WBuffer const& Buffer)
{
	std::lock_guard     Lock(DataMutex);
	WTrafficTreeUpdates Updates{};
	std::stringstream   ss;
	ss.write(Buffer.GetData(), static_cast<long int>(Buffer.GetWritePos()));
	{
		ss.seekg(1); // Skip message type
		cereal::BinaryInputArchive iar(ss);
		iar(Updates);
	}

	for (auto const& MarkedId : Updates.MarkedForRemovalItems)
	{
		MarkedForRemovalItems.insert(MarkedId);

		auto It = TrafficItems.find(MarkedId);
		if (It != TrafficItems.end())
		{
			auto& Item = It->second;
			Item->DownloadSpeed = 0;
			Item->UploadSpeed = 0;

			if (Item->GetType() == TI_Socket)
			{
				if (auto SocketItem = std::dynamic_pointer_cast<WSocketItem>(Item))
				{
					SocketItem->ConnectionState = ESocketConnectionState::Closed;
				}
			}
		}
	}

	for (auto const& RemovedId : Updates.RemovedItems)
	{
		if (TrafficItems.contains(RemovedId))
		{
			TrafficItems.erase(RemovedId);
		}
		RemoveTrafficItem(RemovedId);
	}

	for (auto const& Addition : Updates.AddedSockets)
	{
		// Check if the socket already exists
		if (TrafficItems.contains(Addition.ItemId))
		{
			spdlog::warn("{} already exists in traffic tree, skipping addition", Addition.ItemId);
			continue;
		}
		// Find parent application
		auto                              AppIt = Root->Applications.find(Addition.ApplicationPath);
		std::shared_ptr<WApplicationItem> App;
		if (AppIt == Root->Applications.end())
		{
			// Add new application if not found
			App = std::make_shared<WApplicationItem>();
			App->ApplicationName = Addition.ApplicationName;
			App->ItemId = Addition.ApplicationItemId;
			App->ApplicationPath = Addition.ApplicationPath;
			Root->Applications[Addition.ApplicationPath] = App;
			TrafficItems[App->ItemId] = App;
			if (!ResolvedAddresses.contains(Addition.SocketTuple.RemoteEndpoint.Address))
			{
				ResolvedAddresses[Addition.SocketTuple.RemoteEndpoint.Address] = Addition.ResolvedAddress;
			}
		}
		else
		{
			App = AppIt->second;
		}

		// Find parent process
		auto ProcIt = App->Processes.find(Addition.ProcessId);
		if (ProcIt == App->Processes.end())
		{
			// Add new process if not found
			auto NewProc = std::make_shared<WProcessItem>();
			NewProc->ProcessId = Addition.ProcessId;
			NewProc->ItemId = Addition.ProcessItemId;
			App->Processes[Addition.ProcessId] = NewProc;
			ProcIt = App->Processes.find(Addition.ProcessId);
			TrafficItems[NewProc->ItemId] = NewProc;
		}

		auto const& Proc = ProcIt->second;

		// Create new socket item
		auto NewSocket = std::make_shared<WSocketItem>();
		NewSocket->ItemId = Addition.ItemId;
		NewSocket->SocketTuple = Addition.SocketTuple;
		NewSocket->ConnectionState = Addition.ConnectionState;
		Proc->Sockets[Addition.ItemId] = NewSocket;
		TrafficItems[Addition.ItemId] = NewSocket;
	}

	for (auto const& Addition : Updates.AddedTuples)
	{
		// Check if the tuple already exists
		if (TrafficItems.contains(Addition.ItemId))
		{
			spdlog::warn("{} already exists in traffic tree, skipping addition", Addition.ItemId);
			continue;
		}

		auto SockIt = TrafficItems.find(Addition.SocketItemId);
		if (SockIt == TrafficItems.end() || SockIt->second->GetType() != TI_Socket)
		{
			spdlog::warn(
				"Parent socket {} for tuple {} not found, skipping addition", Addition.SocketItemId, Addition.ItemId);
			continue;
		}

		if (auto SocketItem = std::dynamic_pointer_cast<WSocketItem>(SockIt->second))
		{
			auto NewTuple = std::make_shared<WTupleItem>();
			NewTuple->ItemId = Addition.ItemId;
			SocketItem->UDPPerConnectionTraffic[Addition.Endpoint] = NewTuple;
			TrafficItems[Addition.ItemId] = NewTuple;

			if (!ResolvedAddresses.contains(Addition.Endpoint.Address))
			{
				ResolvedAddresses[Addition.Endpoint.Address] = Addition.ResolvedAddress;
			}
		}
	}

	for (auto const& StateChange : Updates.SocketStateChange)
	{
		auto It = TrafficItems.find(StateChange.ItemId);
		if (It != TrafficItems.end() && It->second->GetType() == TI_Socket)
		{
			if (auto SocketItem = std::dynamic_pointer_cast<WSocketItem>(It->second))
			{
				SocketItem->ConnectionState = StateChange.NewState;
				SocketItem->SocketType = StateChange.SocketType;
				if (StateChange.SocketTuple.has_value())
				{
					SocketItem->SocketTuple = StateChange.SocketTuple.value();
				}
			}
		}
	}

	for (auto const& Update : Updates.UpdatedItems)
	{
		if (auto const It = TrafficItems.find(Update.ItemId); It != TrafficItems.end())
		{
			It->second->DownloadSpeed = Update.NewDownloadSpeed;
			It->second->UploadSpeed = Update.NewUploadSpeed;
			It->second->TotalDownloadBytes = Update.TotalDownloadBytes;
			It->second->TotalUploadBytes = Update.TotalUploadBytes;

			if (Update.ItemId == 0)
			{
				WGlfwWindow::GetInstance().GetMainWindow()->GetNetworkGraphWindow().AddData(
					It->second->UploadSpeed, It->second->DownloadSpeed);
			}
		}
	}
}

static std::string GetSocketName(WSocketItem* Socket)
{
	if (Socket->SocketType == ESocketType::Listen && !Socket->SocketTuple.LocalEndpoint.Address.IsZero())
	{
		return fmt::format("● {}", Socket->SocketTuple.LocalEndpoint.ToString());
	}
	if (Socket->SocketType & ESocketType::Listen && Socket->SocketType & ESocketType::Connect
		&& !Socket->SocketTuple.LocalEndpoint.Address.IsZero() && !Socket->SocketTuple.RemoteEndpoint.Address.IsZero())
	{
		return fmt::format(
			"● {} → {}", Socket->SocketTuple.LocalEndpoint.ToString(), Socket->SocketTuple.RemoteEndpoint.ToString());
	}
	if (Socket->SocketType == ESocketType::Connect)
	{
		if (!Socket->SocketTuple.RemoteEndpoint.Address.IsZero())
		{
			return fmt::format("→ {}", Socket->SocketTuple.RemoteEndpoint.ToString());
		}
		if (!Socket->SocketTuple.LocalEndpoint.Address.IsZero())
		{
			return fmt::format("{}", Socket->SocketTuple.LocalEndpoint.ToString());
		}
	}
	if (Socket->SocketType == ESocketType::Accept)
	{
		return fmt::format("← {}", Socket->SocketTuple.RemoteEndpoint.ToString());
	}

	if (!Socket->SocketTuple.LocalEndpoint.Address.IsZero() && Socket->SocketTuple.Protocol == EProtocol::UDP)
	{
		return fmt::format("○ {}", Socket->SocketTuple.LocalEndpoint.ToString());
	}
	return fmt::format("Socket {}", Socket->ItemId);
}

void WTrafficTree::Draw(ImGuiID MainID)
{
	std::lock_guard Lock(DataMutex);

	ImGui::SetNextWindowDockID(MainID, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Traffic Tree", nullptr, ImGuiWindowFlags_None))
	{
		ImGui::End();
		return;
	}

	// Toolbar
	DrawUnitCombo(WSettings::GetInstance().TrafficTreeUnitSetting);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(200.0f);
	ImGui::InputTextWithHint("##search", "Search applications...", SearchBuffer, sizeof(SearchBuffer));

	ImGui::Separator();
	ImGui::BeginChild("TreeRegion", ImVec2(0, 0), false);

	// 5 columns: Name | upload | download | rule widget
	if (!ImGui::BeginTable("TrafficTable", 4,
			ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV
				| ImGuiTableFlags_ScrollY))
	{
		ImGui::EndChild();
		ImGui::End();
		return;
	}

	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("Dl.", ImGuiTableColumnFlags_WidthFixed, 100.0f);
	ImGui::TableSetupColumn("Ul.", ImGuiTableColumnFlags_WidthFixed, 100.0f);
	ImGui::TableSetupColumn("Rules", ImGuiTableColumnFlags_WidthFixed, 100.0f);

	bool bOpened{};

	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	// start first data row for the root item
	ImGui::TableNextRow();
	bOpened = RenderItem({ Root->HostName, Root, ImGuiTreeNodeFlags_DefaultOpen });
	bool rootOpened = bOpened;

	if (!bOpened)
	{
		ImGui::EndTable();
		ImGui::EndChild();
		ImGui::End();
		return;
	}

	for (auto const& [Name, Child] : Root->Applications)
	{
		if (!Child)
		{
			spdlog::warn("Invalid app {} in traffic tree", Name);
			continue;
		}

		if (Child->NoChildren() && !WSettings::GetInstance().bShowOfflineProcesses)
		{
			continue;
		}

		// Filter by search term
		if (SearchBuffer[0] != '\0')
		{
			std::string AppNameLower = Child->ApplicationName;
			std::string SearchLower = SearchBuffer;

			// Convert both to lowercase for case-insensitive search
			std::transform(AppNameLower.begin(), AppNameLower.end(), AppNameLower.begin(), ::tolower);
			std::transform(SearchLower.begin(), SearchLower.end(), SearchLower.begin(), ::tolower);

			// Skip this application if it doesn't match the search
			if (AppNameLower.find(SearchLower) == std::string::npos)
			{
				continue;
			}
		}

		ImGui::TableNextRow();
		// Stable ID: application name within root
		ImGui::PushID(Name.c_str());

		WRenderItemArgs Args{};
		Args.Name = Child->ApplicationName;
		Args.Item = Child;
		Args.NodeFlags = ImGuiTreeNodeFlags_DefaultOpen;
		Args.ParentApp = Child;
		bOpened = RenderItem(Args);

		if (!bOpened)
		{
			ImGui::PopID();
			continue;
		}

		for (auto const& [PID, Process] : Child->Processes)
		{
			if (Process->Sockets.empty() && !WSettings::GetInstance().bShowOfflineProcesses)
			{
				continue;
			}

			ImGui::TableNextRow();
			ImGui::PushID(PID); // PID fits in int on Linux
			Args = {};
			Args.Name = fmt::format("Process {}", PID);
			Args.Item = Process;
			Args.bMarkedForRemoval = MarkedForRemovalItems.contains(Process->ItemId);
			Args.ParentApp = Child;
			bOpened = RenderItem(Args);

			if (!bOpened)
			{
				ImGui::PopID();
				continue;
			}

			for (auto const& [SocketCookie, Socket] : Process->Sockets)
			{
				if (Socket->SocketType == ESocketType::Unknown && !WSettings::GetInstance().bShowUninitalizedSockets)
				{
					continue;
				}

				ImGui::TableNextRow();
				// Use string ID for potentially wide cookies
				std::string SockId = std::string("sock:") + std::to_string(SocketCookie);
				ImGui::PushID(SockId.c_str());
				ImGuiTreeNodeFlags SocketFlags = 0;
				auto               SocketName = GetSocketName(Socket.get());

				if (Socket->UDPPerConnectionTraffic.empty())
				{
					SocketFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
				}

				auto const bPendingRemoval = MarkedForRemovalItems.contains(Socket->ItemId);
				Args = {};
				Args.Name = SocketName;
				Args.Item = Socket;
				Args.NodeFlags = SocketFlags;
				Args.bMarkedForRemoval = bPendingRemoval;
				Args.ParentApp = Child;
				bOpened = RenderItem(Args);
				if (!bOpened)
				{
					ImGui::PopID();
					continue;
				}

				for (auto const& [TupleKey, Tuple] : Socket->UDPPerConnectionTraffic)
				{
					ImGui::TableNextRow();
					std::string tupleId = std::string("tuple:") + TupleKey.ToString();
					ImGui::PushID(tupleId.c_str());
					Args = {};
					Args.Name = fmt::format("↔ {}", TupleKey.ToString());
					Args.Item = Tuple;
					Args.NodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
					Args.bMarkedForRemoval = bPendingRemoval;
					Args.TupleEndpoint = &TupleKey;
					RenderItem(Args);

					ImGui::PopID();
				}
				ImGui::PopID();

				if (!Socket->UDPPerConnectionTraffic.empty())
				{
					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
			ImGui::PopID(); // PID
		}

		ImGui::TreePop();
		ImGui::PopID(); // app name
	}

	// close root tree node if it was opened
	if (rootOpened)
		ImGui::TreePop();

	ImGui::EndTable();
	ImGui::EndChild();
	ImGui::End();
}

void WTrafficTree::SetResolvedAddresses(WBuffer const& Buffer)
{
	std::unordered_map<WIPAddress, std::string> NewResolvedAddresses{};
	std::stringstream                           SS;
	SS.write(Buffer.GetData(), static_cast<long int>(Buffer.GetWritePos()));
	{
		SS.seekg(1); // Skip message type
		cereal::BinaryInputArchive Iar(SS);
		Iar(NewResolvedAddresses);
	}
	std::lock_guard Lock(DataMutex);
	ResolvedAddresses = std::move(NewResolvedAddresses);
}