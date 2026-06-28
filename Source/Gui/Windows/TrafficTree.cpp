/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "TrafficTree.hpp"

#include <ranges>
#include <algorithm>

#include "imgui.h"
#include "spdlog/spdlog.h"
#include "spdlog/fmt/bundled/chrono.h"
#include "cereal/types/optional.hpp"
#include "cereal/types/array.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/unordered_map.hpp"
// ReSharper disable CppUnusedIncludeDirective
#include "cereal/types/memory.hpp"
#include "cereal/types/string.hpp"
// ReSharper enable CppUnusedIncludeDirective

#include "AppIconAtlas.hpp"
#include "ClientRuleManager.hpp"
#include "Util/ImGuiUtil.hpp"
#include "Format.hpp"
#include "SdlWindow.hpp"
#include "Messages.hpp"
#include "Data/ResolveData.hpp"
#include "Data/TrafficTreeUpdate.hpp"
#include "Icons/IconAtlas.hpp"
#include "Util/I18n.hpp"
#include "Util/Settings.hpp"

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
	auto const IconSize = ImGui::CalcTextSize(Name.c_str()).y;
	if (Item->GetType() == TI_Application)
	{
		std::lock_guard Lock(WAppIconAtlas::GetInstance().GetMutex());
		WAppIconAtlas::GetInstance().DrawIconForApplication(
			std::dynamic_pointer_cast<WApplicationItem>(Item)->ApplicationName, ImVec2{ IconSize, IconSize });
	}
	else if (Item->GetType() == TI_Process)
	{
		WIconAtlas::GetInstance().DrawIcon("process", ImVec2{ IconSize, IconSize });
	}
	else if (Item->GetType() == TI_Filter)
	{
		WIconAtlas::GetInstance().DrawIcon("filter", ImVec2{ IconSize, IconSize });
	}
	else
	{
		WIconAtlas::GetInstance().DrawIcon("computer", ImVec2{ IconSize, IconSize });
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
		if (Args.Item->GetType() == TI_Socket)
		{
			auto const Socket = std::static_pointer_cast<WSocketItem>(Args.Item);
			ImGui::Text("ID: %llu, Cookie: %lu", Args.Item->ItemId, Socket->Cookie);
		}
		else
		{
			ImGui::Text("ID: %llu", Args.Item->ItemId);
		}

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

void SortNode(std::shared_ptr<WTreeNode> const& Node, ImS16 const Column, ImGuiSortDirection const Direction)
{
	auto Less = [&](std::shared_ptr<WTreeNode> const& A, std::shared_ptr<WTreeNode> const& B) {
		switch (Column)
		{
			case 0: // Name
				return WStringFormat::ToLower(A->Item->ToString()) < WStringFormat::ToLower(B->Item->ToString());
			case 1: // Download
				return A->Item->DownloadSpeed < B->Item->DownloadSpeed;
			case 2: // Upload
				return A->Item->UploadSpeed < B->Item->UploadSpeed;
			default:
				return false;
		}
	};

	std::ranges::sort(Node->Children, [&](std::shared_ptr<WTreeNode> const& A, std::shared_ptr<WTreeNode> const& B) {
		if (Direction == ImGuiSortDirection_Ascending)
		{
			return Less(A, B);
		}
		return Less(B, A);
	});

	for (auto& Child : Node->Children)
		SortNode(Child, Column, Direction);
}

void WTrafficTree::SortTree(ImGuiTableSortSpecs const* Specs)
{
#if WDEBUG
	WStopwatch SortTimer;
#endif
	if (Specs->SpecsCount == 0)
	{
		return;
	}
	TreeRoot = std::make_shared<WTreeNode>();
	// Fill up root
	// for (auto const& Filter : Root->Filters)
	// {
	// 	auto Item = std::make_shared<WTreeNode>();
	// 	Item->Item = Filter;
	// 	TreeRoot->Children.push_back(Item);
	// }
	for (auto const& App : Root->Applications | std::views::values)
	{
		auto AppNode = std::make_shared<WTreeNode>();
		AppNode->Item = App;
		TreeRoot->Children.push_back(AppNode);

		for (auto const& Process : App->Processes | std::views::values)
		{
			auto ProcNode = std::make_shared<WTreeNode>();
			ProcNode->Item = Process;
			AppNode->Children.push_back(ProcNode);

			for (auto const& Socket : Process->Sockets | std::views::values)
			{
				auto SocketNode = std::make_shared<WTreeNode>();
				SocketNode->Item = Socket;
				ProcNode->Children.push_back(SocketNode);
				for (auto const& Tuple : Socket->UDPPerConnectionTraffic)
				{
					auto UDPNode = std::make_shared<WTreeNode>();
					UDPNode->Item = Tuple;
					SocketNode->Children.push_back(UDPNode);
				}
			}
		}
	}

	auto const& ColSpec = Specs->Specs[0];
	SortNode(TreeRoot, ColSpec.ColumnIndex, ColSpec.SortDirection);
#if WDEBUG
	spdlog::debug("Sorting took {} ms", SortTimer.ElapsedMs());
#endif
}

void WTrafficTree::LoadFromBuffer(WBuffer const& Buffer)
{
	std::lock_guard Lock(DataMutex);
	TrafficItems.clear();
	if (!DeserializeMessage(Buffer, *Root.get()))
	{
		spdlog::error("Failed to deserialize traffic tree");
		return;
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
				for (auto const& UDPTuple : Sock->UDPPerConnectionTraffic)
				{
					TrafficItems[UDPTuple->ItemId] = UDPTuple;
				}
			}
		}
	}

	for (auto const& Filter : Root->Filters)
	{
		TrafficItems[Filter->ItemId] = Filter;
	}

	TrafficItems[0] = Root;
	if (auto const* Sort = ImGui::TableGetSortSpecs())
	{
		SortTree(Sort);
	}
}

void WTrafficTree::UpdateFromBuffer(WBuffer const& Buffer)
{
	std::lock_guard     Lock(DataMutex);
	WTrafficTreeUpdates Updates{};
	if (!DeserializeMessage(Buffer, Updates))
	{
		spdlog::error("Failed to deserialize traffic tree update");
		return;
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
		if (TrafficItems.contains(Addition.ItemId))
		{
			spdlog::warn("{} already exists in traffic tree, skipping addition", Addition.ItemId);
			continue;
		}
		auto                              AppIt = Root->Applications.find(Addition.ApplicationPath);
		std::shared_ptr<WApplicationItem> App;
		if (AppIt == Root->Applications.end())
		{
			App = std::make_shared<WApplicationItem>();
			App->ApplicationName = Addition.ApplicationName;
			App->ItemId = Addition.ApplicationItemId;
			App->ApplicationPath = Addition.ApplicationPath;
			Root->Applications[Addition.ApplicationPath] = App;
			TrafficItems[App->ItemId] = App;
		}
		else
		{
			App = AppIt->second;
		}

		auto ProcIt = App->Processes.find(Addition.ProcessId);
		if (ProcIt == App->Processes.end())
		{
			auto NewProc = std::make_shared<WProcessItem>();
			NewProc->ProcessId = Addition.ProcessId;
			NewProc->ItemId = Addition.ProcessItemId;
			App->Processes[Addition.ProcessId] = NewProc;
			ProcIt = App->Processes.find(Addition.ProcessId);
			TrafficItems[NewProc->ItemId] = NewProc;
		}

		auto const& Proc = ProcIt->second;

		auto NewSocket = std::make_shared<WSocketItem>();
		NewSocket->ItemId = Addition.ItemId;
		NewSocket->SocketTuple = Addition.SocketTuple;
		NewSocket->ConnectionState = Addition.ConnectionState;
		NewSocket->SocketType = Addition.SocketType;
		NewSocket->Cookie = Addition.SocketCookie;
		Proc->Sockets[Addition.ItemId] = NewSocket;
		TrafficItems[Addition.ItemId] = NewSocket;
	}

	for (auto const& Addition : Updates.AddedTuples)
	{
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
			SocketItem->UDPPerConnectionTraffic.emplace_back(NewTuple);
			TrafficItems[Addition.ItemId] = NewTuple;
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
				if (StateChange.SocketTuple)
				{
					SocketItem->SocketTuple = *StateChange.SocketTuple.get();
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
				WSdlWindow::GetInstance().GetMainWindow()->GetNetworkGraphWindow().AddData(
					It->second->UploadSpeed, It->second->DownloadSpeed);
			}
		}
	}
	bRequireTreeSorting = true;
}

static std::string GetSocketName(WSocketItem* Socket)
{
	if (Socket->SocketType == ESocketType::Listen && Socket->SocketTuple.LocalEndpoint.Port != 0)
	{
		return std::format("● {}", Socket->SocketTuple.LocalEndpoint.ToString());
	}
	if (Socket->SocketType & ESocketType::Listen && Socket->SocketType & ESocketType::Connect
		&& !Socket->SocketTuple.LocalEndpoint.Address.IsZero() && !Socket->SocketTuple.RemoteEndpoint.Address.IsZero())
	{
		return std::format(
			"● {} → {}", Socket->SocketTuple.LocalEndpoint.ToString(), Socket->SocketTuple.RemoteEndpoint.ToString());
	}
	if (Socket->SocketType == ESocketType::Connect)
	{
		if (!Socket->SocketTuple.RemoteEndpoint.Address.IsZero())
		{
			return std::format("→ {}", Socket->SocketTuple.RemoteEndpoint.ToString());
		}
		if (Socket->SocketTuple.LocalEndpoint.Port != 0)
		{
			return std::format("{} →", Socket->SocketTuple.LocalEndpoint.ToString());
		}
	}
	if (Socket->SocketType == ESocketType::Accept)
	{
		return std::format("← {}", Socket->SocketTuple.RemoteEndpoint.ToString());
	}

	if (!Socket->SocketTuple.LocalEndpoint.Address.IsZero() && Socket->SocketTuple.Protocol == EProtocol::UDP)
	{
		return std::format("○ {}", Socket->SocketTuple.LocalEndpoint.ToString());
	}
	return std::format("Socket {}", Socket->ItemId);
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
	ImGui::InputTextWithHint("##search", TR("__search.apps"), SearchBuffer, sizeof(SearchBuffer));

	ImGui::Separator();
	ImGui::BeginChild("TreeRegion", ImVec2(0, 0), false);

	// 5 columns: Name | upload | download | rule widget
	if (!ImGui::BeginTable("TrafficTable", 4,
			ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY
				| ImGuiTableFlags_Sortable))
	{
		ImGui::EndChild();
		ImGui::End();
		return;
	}

	ImGui::TableSetupColumn(TR("name"), ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("▼", ImGuiTableColumnFlags_WidthFixed, 100.0f);
	ImGui::TableSetupColumn("▲", ImGuiTableColumnFlags_WidthFixed, 100.0f);
	ImGui::TableSetupColumn(TR("rules"), ImGuiTableColumnFlags_WidthFixed, 100.0f);

	bool bOpened{};

	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	if (auto* Sort = ImGui::TableGetSortSpecs(); Sort && !ImGui::GetIO().KeyCtrl)
	{
		if (Sort->SpecsDirty || bRequireTreeSorting)
		{
			SortTree(Sort);
			Sort->SpecsDirty = false;
			bRequireTreeSorting = false;
		}
	}

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

	// Draw filters first always
	for (auto const& Filter : Root->Filters)
	{
		ImGui::TableNextRow();
		ImGui::PushID(Filter->ItemId);
		WRenderItemArgs Args;
		Args.Name = Filter->Name;
		Args.Item = Filter;
		Args.NodeFlags = ImGuiTreeNodeFlags_Leaf;
		RenderItem(Args);
		ImGui::PopID();
		ImGui::TreePop();
	}

	for (auto const& AppNode : TreeRoot->Children)
	{
		if (!AppNode->Item || AppNode->Item->GetType() != TI_Application)
		{
			spdlog::warn("Invalid app in traffic tree");
			continue;
		}

		if (!AppNode->Item->HasChildren() && !WSettings::GetInstance().bShowOfflineProcesses)
		{
			continue;
		}
		auto AppItem = std::static_pointer_cast<WApplicationItem>(AppNode->Item);

		if (SearchBuffer[0] != '\0')
		{
			std::string AppNameLower = AppItem->ApplicationName;
			std::string SearchLower = SearchBuffer;

			std::ranges::transform(AppNameLower, AppNameLower.begin(), ::tolower);
			std::ranges::transform(SearchLower, SearchLower.begin(), ::tolower);

			if (AppNameLower.find(SearchLower) == std::string::npos)
			{
				continue;
			}
		}

		ImGui::TableNextRow();
		// Stable ID: application name within root
		ImGui::PushID(AppItem->ApplicationName.c_str());

		WRenderItemArgs Args{};
		Args.Name = AppItem->ApplicationName;
		Args.Item = AppItem;
		Args.NodeFlags = ImGuiTreeNodeFlags_DefaultOpen;
		Args.ParentApp = AppItem;
		bOpened = RenderItem(Args);

		if (!bOpened)
		{
			ImGui::PopID();
			continue;
		}

		for (auto const& ProcNode : AppNode->Children)
		{
			auto ProcItem = std::static_pointer_cast<WProcessItem>(ProcNode->Item);
			if (ProcItem->Sockets.empty() && !WSettings::GetInstance().bShowOfflineProcesses)
			{
				continue;
			}

			ImGui::TableNextRow();
			ImGui::PushID(ProcItem->ProcessId);
			Args = {};
			Args.Name = std::format("{} {}", TR("__process"), ProcItem->ProcessId);
			Args.Item = ProcItem;
			Args.bMarkedForRemoval = MarkedForRemovalItems.contains(ProcItem->ItemId);
			Args.ParentApp = AppItem;
			if (!ProcItem->HasChildren())
			{
				Args.NodeFlags |= ImGuiTreeNodeFlags_Leaf;
			}
			bOpened = RenderItem(Args);

			if (!bOpened)
			{
				ImGui::PopID();
				continue;
			}

			for (auto const& SocketNode : ProcNode->Children)
			{
				auto SocketItem = std::static_pointer_cast<WSocketItem>(SocketNode->Item);
				if (SocketItem->SocketType == ESocketType::Unknown
					&& !WSettings::GetInstance().bShowUninitalizedSockets)
				{
					continue;
				}

				ImGui::TableNextRow();
				std::string SockId = std::string("sock:") + std::to_string(SocketItem->Cookie);
				ImGui::PushID(SockId.c_str());
				ImGuiTreeNodeFlags SocketFlags = 0;
				auto               SocketName = GetSocketName(SocketItem.get());

				if (SocketItem->UDPPerConnectionTraffic.empty())
				{
					SocketFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
				}

				auto const bPendingRemoval = MarkedForRemovalItems.contains(SocketItem->ItemId);
				Args = {};
				Args.Name = SocketName;
				Args.Item = SocketItem;
				Args.NodeFlags = SocketFlags;
				Args.bMarkedForRemoval = bPendingRemoval;
				Args.ParentApp = AppItem;
				bOpened = RenderItem(Args);
				if (!bOpened)
				{
					ImGui::PopID();
					continue;
				}

				for (auto const& UDPNode : SocketNode->Children)
				{
					auto UDPItem = std::static_pointer_cast<WTupleItem>(UDPNode->Item);
					ImGui::TableNextRow();
					std::string tupleId = std::string("tuple:") + std::to_string(UDPItem->ItemId);
					ImGui::PushID(tupleId.c_str());
					Args = {};
					Args.Name = std::format("↔ {}", UDPNode->TupleEndpoint.ToString());
					Args.Item = UDPItem;
					Args.NodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
					Args.bMarkedForRemoval = bPendingRemoval;
					Args.TupleEndpoint = &UDPNode->TupleEndpoint;
					RenderItem(Args);

					ImGui::PopID();
				}
				ImGui::PopID();

				if (!SocketItem->UDPPerConnectionTraffic.empty())
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

void WTrafficTree::HandleResolveResponse(WBuffer const& Buffer)
{
	WResolveResponse ResolveResponse{};
	if (WClient::ReadMessage(Buffer, ResolveResponse))
	{
		std::lock_guard Lock(DataMutex);
		ResolvedAddresses[ResolveResponse.AddressToResolve] = ResolveResponse.ResolveResult;
	}
}

std::string const& WTrafficTree::ResolveAddress(WIPAddress const& Address)
{
	static std::string Empty{};
	std::scoped_lock   Lock(DataMutex);
	if (auto It = ResolvedAddresses.find(Address); It != ResolvedAddresses.end())
	{
		return It->second;
	}
	ResolvedAddresses[Address] = Empty;
	WResolveRequest Request;
	Request.AddressToResolve = Address;
	WClient::GetInstance().SendMessage(MT_ResolveRequest, Request);
	return Empty;
}
