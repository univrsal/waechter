//
// Created by usr on 28/10/2025.
//

#include "TrafficTree.hpp"

// ReSharper disable CppUnusedIncludeDirective

#include <imgui.h>
#include <cereal/types/optional.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/archives/binary.hpp>

#include "Format.hpp"
#include "GlfwWindow.hpp"
#include "Messages.hpp"
#include "Data/TrafficTreeUpdate.hpp"

#include <ranges>

template <class K, class V>
static bool TryRemoveFromMap(std::unordered_map<K, V>& Map, WTrafficItemId TrafficItemId)
{
	for (auto It = Map.begin(); It != Map.end(); ++It)
	{
		if (It->second->ItemId == TrafficItemId)
		{
			Map.erase(It);
			return true;
		}
	}
	return false;
}

// This isn't exactly efficient, we should probably have something
// along the lines of ITrafficItem->Parent->RemoveChild or similar.
void WTrafficTree::RemoveTrafficItem(WTrafficItemId TrafficItemId)
{
	MarkedForRemovalItems.erase(TrafficItemId);
	if (Root.RemoveChild(TrafficItemId))
	{
		return;
	}

	for (auto& App : Root.Applications | std::views::values)
	{
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

bool WTrafficTree::RenderItem(
	std::string const& Name, ITrafficItem const* Item, ImGuiTreeNodeFlags NodeFlags, ETrafficItemType Type)
{
	NodeFlags |= ImGuiTreeNodeFlags_SpanFullWidth;
	NodeFlags |= ImGuiTreeNodeFlags_OpenOnArrow;
	ImGui::TableSetColumnIndex(0);

	if (MarkedForRemovalItems.contains(Item->ItemId))
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{ 1.0f, 0.4f, 0.4f, 1.0f });
	}

	if (SelectedItemId == Item->ItemId)
	{
		NodeFlags |= ImGuiTreeNodeFlags_Selected;
	}

	auto bNodeOpen = ImGui::TreeNodeEx(Name.c_str(), NodeFlags);

	if (MarkedForRemovalItems.contains(Item->ItemId))
	{
		ImGui::PopStyleColor();
	}

	if (ImGui::IsItemClicked())
	{
		SelectedItemId = Item->ItemId;
		SelectedItem = Item;
		SelectedItemType = Type;
	}

#if WDEBUG
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::Text("ID: %llu", Item->ItemId);
		ImGui::EndTooltip();
	}
#endif

	if (Item->DownloadSpeed > 0)
	{
		ImGui::TableSetColumnIndex(1);
		ImGui::Text("%s", WTrafficFormat::Format(Item->DownloadSpeed, Unit).c_str());
	}

	if (Item->UploadSpeed > 0)
	{
		ImGui::TableSetColumnIndex(2);
		ImGui::Text("%s", WTrafficFormat::Format(Item->UploadSpeed, Unit).c_str());
	}

	ImGui::TableSetColumnIndex(3);
	// placeholder: add a DownloadLimit field to WSystemItem to show real values
	ImGui::TextDisabled("-");

	ImGui::TableSetColumnIndex(4);
	// placeholder: add a UploadLimit field to WSystemItem to show real values
	ImGui::TextDisabled("-");
	return bNodeOpen;
}

void WTrafficTree::LoadFromBuffer(WBuffer const& Buffer)
{
	TrafficItems.clear();
	std::stringstream ss;
	ss.write(Buffer.GetData(), static_cast<long int>(Buffer.GetWritePos()));
	{
		ss.seekg(1); // Skip message type
		cereal::BinaryInputArchive iar(ss);
		iar(Root);
	}

	for (auto const& App : Root.Applications | std::views::values)
	{
		TrafficItems[App->ItemId] = App.get();
		for (auto const& Proc : App->Processes | std::views::values)
		{
			TrafficItems[Proc->ItemId] = Proc.get();
			for (auto const& Sock : Proc->Sockets | std::views::values)
			{
				TrafficItems[Sock->ItemId] = Sock.get();
			}
		}
	}

	TrafficItems[0] = &Root;
}

void WTrafficTree::UpdateFromBuffer(WBuffer const& Buffer)
{
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
				auto SocketItem = static_cast<WSocketItem*>(Item);
				SocketItem->ConnectionState = ESocketConnectionState::Closed;
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

	for (auto const& Addition : Updates.AddedSockets)
	{
		// Check if the socket already exists
		if (TrafficItems.contains(Addition.ItemId))
		{
			spdlog::warn("{} already exists in traffic tree, skipping addition", Addition.ItemId);
			continue;
		}
		// Find parent application
		auto AppIt = Root.Applications.find(Addition.ApplicationPath);
		if (AppIt == Root.Applications.end())
		{
			// Add new application if not found
			auto NewApp = std::make_shared<WApplicationItem>();
			NewApp->ApplicationName = Addition.ApplicationName;
			NewApp->ItemId = Addition.ApplicationItemId;
			NewApp->ApplicationPath = Addition.ApplicationPath;
			Root.Applications[Addition.ApplicationPath] = NewApp;
			AppIt = Root.Applications.find(Addition.ApplicationPath);
			TrafficItems[NewApp->ItemId] = NewApp.get();
		}
		auto& App = AppIt->second;

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
			TrafficItems[NewProc->ItemId] = NewProc.get();
		}

		auto const& Proc = ProcIt->second;

		// Create new socket item
		auto NewSocket = std::make_shared<WSocketItem>();
		NewSocket->ItemId = Addition.ItemId;
		NewSocket->SocketTuple = Addition.SocketTuple;
		NewSocket->ConnectionState = Addition.ConnectionState;
		Proc->Sockets[Addition.ItemId] = NewSocket;
		TrafficItems[Addition.ItemId] = NewSocket.get();
	}

	for (auto const& StateChange : Updates.SocketStateChange)
	{
		auto It = TrafficItems.find(StateChange.ItemId);
		if (It != TrafficItems.end() && It->second->GetType() == TI_Socket)
		{
			if (auto SocketItem = dynamic_cast<WSocketItem*>(It->second))
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
}

static std::string GetSocketName(WSocketItem* Socket)
{
	if (Socket->SocketType == ESocketType::Listen && !Socket->SocketTuple.LocalEndpoint.Address.IsZero())
	{
		return fmt::format("● {}", Socket->SocketTuple.LocalEndpoint.ToString());
	}
	if (Socket->SocketType == ESocketType::Connect && !Socket->SocketTuple.LocalEndpoint.Address.IsZero())
	{
		return fmt::format("→ {}", Socket->SocketTuple.RemoteEndpoint.ToString());
	}
	if (Socket->SocketType == ESocketType::Accept)
	{
		return fmt::format("← {}", Socket->SocketTuple.RemoteEndpoint.ToString());
	}
	return fmt::format("Socket {}", Socket->ItemId);
}

void WTrafficTree::Draw(ImGuiID MainID)
{
	auto UnitText = [this] {
		switch (Unit)
		{
			case TU_Bps:
				return "B/s";
			case TU_KiBps:
				return "KiB/s";
			case TU_MiBps:
				return "MiB/s";
			case TU_GiBps:
				return "GiB/s";
			case TU_Auto:
				return "Auto";
			default:
				return "B/s";
		}
	};

	ImGui::SetNextWindowDockID(MainID, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Traffic Tree", nullptr, ImGuiWindowFlags_None))
	{
		ImGui::End();
		return;
	}

	// Toolbar
	if (ImGui::BeginCombo("Unit", UnitText(), ImGuiComboFlags_WidthFitPreview))
	{
		if (ImGui::Selectable("B/s", Unit == TU_Bps))
		{
			Unit = TU_Bps;
		}
		if (ImGui::Selectable("KiB/s", Unit == TU_KiBps))
		{
			Unit = TU_KiBps;
		}
		if (ImGui::Selectable("MiB/s", Unit == TU_MiBps))
		{
			Unit = TU_MiBps;
		}
		if (ImGui::Selectable("GiB/s", Unit == TU_GiBps))
		{
			Unit = TU_GiBps;
		}
		if (ImGui::Selectable("Auto", Unit == TU_Auto))
		{
			Unit = TU_Auto;
		}
		ImGui::EndCombo();
	}

	ImGui::Separator();
	ImGui::BeginChild("TreeRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

	// 5 columns: Name | upload | download | upload limit | download limit
	if (!ImGui::BeginTable(
			"TrafficTable", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::EndChild();
		ImGui::End();
		return;
	}

	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("Dl.", ImGuiTableColumnFlags_WidthFixed, 100.0f);
	ImGui::TableSetupColumn("Ul.", ImGuiTableColumnFlags_WidthFixed, 100.0f);
	ImGui::TableSetupColumn("Dl. limit", ImGuiTableColumnFlags_WidthFixed, 100.0f);
	ImGui::TableSetupColumn("Ul. limit", ImGuiTableColumnFlags_WidthFixed, 100.0f);

	bool bOpened{};
	ImGui::TableHeadersRow();

	// start first data row for the root item
	ImGui::TableNextRow();
	bOpened = RenderItem(Root.HostName, &Root, ImGuiTreeNodeFlags_DefaultOpen, TI_System);
	bool rootOpened = bOpened;

	if (!bOpened)
	{
		ImGui::EndTable();
		ImGui::EndChild();
		ImGui::End();
		return;
	}

	for (auto const& [Name, Child] : Root.Applications)
	{
		if (Child->NoChildren())
		{
			continue;
		}

		ImGui::TableNextRow();
		// Stable ID: application name within root
		ImGui::PushID(Name.c_str());
		bOpened = RenderItem(Child->ApplicationName, Child.get(), ImGuiTreeNodeFlags_DefaultOpen, TI_Application);

		if (!bOpened)
		{
			ImGui::PopID();
			continue;
		}

		for (auto const& [PID, Process] : Child->Processes)
		{
			if (Process->Sockets.empty())
			{
				continue;
			}

			ImGui::TableNextRow();
			ImGui::PushID(PID); // PID fits in int on Linux
			bOpened = RenderItem(fmt::format("Process {}", PID), Process.get(), 0, TI_Process);

			if (!bOpened)
			{
				ImGui::PopID();
				continue;
			}

			for (auto const& [SocketCookie, Socket] : Process->Sockets)
			{
				ImGui::TableNextRow();
				// Use string ID for potentially wide cookies
				std::string sockId = std::string("sock:") + std::to_string(SocketCookie);
				ImGui::PushID(sockId.c_str());
				ImGuiTreeNodeFlags socketFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
				auto               SocketName = GetSocketName(Socket.get());

				RenderItem(SocketName, Socket.get(), socketFlags, TI_Socket);
				ImGui::PopID();
				// No TreePop() here because NoTreePushOnOpen prevents a push
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