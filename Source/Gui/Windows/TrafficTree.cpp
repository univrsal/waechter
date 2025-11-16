//
// Created by usr on 28/10/2025.
//

#include "TrafficTree.hpp"

// ReSharper disable CppUnusedIncludeDirective
#include <imgui.h>
#include <ranges>
#include <spdlog/spdlog.h>
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
#include "Util/Settings.hpp"

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

bool WTrafficTree::RenderItem(std::string const& Name, std::shared_ptr<ITrafficItem> Item, ImGuiTreeNodeFlags NodeFlags,
	ETrafficItemType Type, WEndpoint const* TupleEndpoint)
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
		if (TupleEndpoint)
		{
			SelectedTupleEndpoint = *TupleEndpoint;
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
	bOpened = RenderItem(Root->HostName, Root, ImGuiTreeNodeFlags_DefaultOpen, TI_System);
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

		if (Child->NoChildren())
		{
			continue;
		}

		ImGui::TableNextRow();
		// Stable ID: application name within root
		ImGui::PushID(Name.c_str());
		bOpened = RenderItem(Child->ApplicationName, Child, ImGuiTreeNodeFlags_DefaultOpen, TI_Application);

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
			bOpened = RenderItem(fmt::format("Process {}", PID), Process, 0, TI_Process);

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

				bOpened = RenderItem(SocketName, Socket, SocketFlags, TI_Socket);

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
					ImGuiTreeNodeFlags tupleFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
					RenderItem(fmt::format("↔ {}", TupleKey.ToString()), Tuple, tupleFlags, TI_Tuple, &TupleKey);
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