//
// Created by usr on 28/10/2025.
//

#include "TrafficTree.hpp"

#include "Format.hpp"

#include <imgui.h>
// ReSharper disable CppUnusedIncludeDirective
#include "Messages.hpp"

#include <cereal/types/array.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/archives/binary.hpp>

void WTrafficTree::LoadFromBuffer(WBuffer const& Buffer)
{
	std::stringstream ss;
	ss.write(Buffer.GetData(), static_cast<long int>(Buffer.GetWritePos()));
	{
		ss.seekg(1); // Skip message type
		cereal::BinaryInputArchive iar(ss);
		iar(Root);
	}
}

void WTrafficTree::UpdateFromBuffer(WBuffer const&)
{
}

void WTrafficTree::Draw()
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

	// 5 columns: Name | upload | download | upload limit | download limit
	if (!ImGui::BeginTable("TrafficTable", 5,
			ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
	{
		return;
	}

	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("Dl.", ImGuiTableColumnFlags_WidthFixed, 100.0f);
	ImGui::TableSetupColumn("Ul.", ImGuiTableColumnFlags_WidthFixed, 100.0f);
	ImGui::TableSetupColumn("Dl. limit", ImGuiTableColumnFlags_WidthFixed, 100.0f);
	ImGui::TableSetupColumn("Ul. limit", ImGuiTableColumnFlags_WidthFixed, 100.0f);

	auto RenderItem = [&](std::string const& Name, const ITrafficItem* Item, ImGuiTreeNodeFlags NodeFlags) {
		ImGui::TableSetColumnIndex(0);
		if (!ImGui::TreeNodeEx(Name.c_str(), NodeFlags))
		{
			return false;
		}

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
		return true;
	};

	bool bOpened{};
	ImGui::TableHeadersRow();

	// start first data row for the root item
	ImGui::TableNextRow();
	bOpened = RenderItem(Root.HostName, &Root, ImGuiTreeNodeFlags_DefaultOpen);
	bool rootOpened = bOpened;

	if (!bOpened)
	{
		ImGui::EndTable();
		return;
	}

	for (const auto& [Name, Child] : Root.Applications)
	{
		ImGui::TableNextRow();
		// Stable ID: application name within root
		ImGui::PushID(Name.c_str());
		bOpened = RenderItem(Name, Child.get(), ImGuiTreeNodeFlags_DefaultOpen);

		if (!bOpened)
		{
			ImGui::PopID();
			continue;
		}

		for (const auto& [PID, Process] : Child->Processes)
		{
			ImGui::TableNextRow();
			ImGui::PushID(PID); // PID fits in int on Linux
			bOpened = RenderItem(fmt::format("Process {}", PID), Process.get(), ImGuiTreeNodeFlags_DefaultOpen);

			if (!bOpened)
			{
				ImGui::PopID();
				continue;
			}

			for (const auto& [SocketCookie, Socket] : Process->Sockets)
			{
				ImGui::TableNextRow();
				// Use string ID for potentially wide cookies
				std::string sockId = std::string("sock:") + std::to_string(SocketCookie);
				ImGui::PushID(sockId.c_str());
				ImGuiTreeNodeFlags socketFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
				RenderItem(fmt::format("Socket {}", SocketCookie), Socket.get(), socketFlags);
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
}