//
// Created by usr on 28/10/2025.
//

#include "TrafficTree.hpp"

#include "Format.hpp"

#include <spdlog/spdlog.h>
#include <imgui.h>

#include "Json.hpp"

void WTrafficTree::LoadFromJson(std::string const& Json)
{
	std::string Err;
	WJson       TreeJson = WJson::parse(Json, Err);

	Root.Name = "System";
	Root.Upload = 0;
	Root.Download = 0;
	if (!Err.empty())
	{
		spdlog::error("Failed to parse traffic tree json: {}", Err);
		return;
	}

	Root.Name = TreeJson[JSON_KEY_HOSTNAME].string_value();
	Root.Upload = TreeJson[JSON_KEY_UPLOAD].number_value();
	Root.Download = TreeJson[JSON_KEY_DOWNLOAD].number_value();

	auto const& AppsJson = TreeJson[JSON_KEY_APPS].array_items();
	for (auto const& App : AppsJson)
	{
		auto AppName = App[JSON_KEY_BINARY_NAME].string_value();
		if (AppName.empty())
		{
			continue;
		}
		auto AppNode = std::make_shared<WTrafficTreeNode>();
		AppNode->Name = AppName;
		AppNode->Tooltip = App[JSON_KEY_BINARY_PATH].string_value();
		AppNode->Upload = App[JSON_KEY_UPLOAD].number_value();
		AppNode->Download = App[JSON_KEY_DOWNLOAD].number_value();

		auto const& ProcsJson = App[JSON_KEY_PROCESSES].array_items();
		for (auto const& Proc : ProcsJson)
		{
			auto ProcNode = std::make_shared<WTrafficTreeNode>();
			ProcNode->Name = fmt::format("PID {}", static_cast<WProcessId>(Proc[JSON_KEY_PID].number_value()));
			ProcNode->Tooltip = Proc[JSON_KEY_CMDLINE].string_value();
			ProcNode->Upload = Proc[JSON_KEY_UPLOAD].number_value();
			ProcNode->Download = Proc[JSON_KEY_DOWNLOAD].number_value();

			auto const& SocketsJson = Proc[JSON_KEY_SOCKETS].array_items();
			for (auto const& Sock : SocketsJson)
			{
				auto SocketNode = std::make_shared<WTrafficTreeNode>();
				auto ID = static_cast<uint64_t>(Sock[JSON_KEY_ID].number_value());
				SocketNode->Name = fmt::format("Socket {}", static_cast<WSocketCookie>(Sock[JSON_KEY_SOCKET_COOKIE].number_value()));
				SocketNode->Upload = Sock[JSON_KEY_UPLOAD].number_value();
				SocketNode->Download = Sock[JSON_KEY_DOWNLOAD].number_value();
				ProcNode->Children.emplace_back(SocketNode);
				Nodes[ID] = SocketNode;
			}
			AppNode->Children.emplace_back(ProcNode);
			auto ID = static_cast<uint64_t>(Proc[JSON_KEY_ID].number_value());
			Nodes[ID] = ProcNode;
		}

		Root.Children.emplace_back(AppNode);
		auto ID = static_cast<uint64_t>(App[JSON_KEY_ID].number_value());
		Nodes[ID] = AppNode;
	}
}

void WTrafficTree::UpdateFromJson(std::string const& Json)
{
	std::string Err;
	auto        Data = WJson::parse(Json, Err);
	if (!Err.empty())
	{
		spdlog::error("Failed to parse traffic tree update json: {}", Err);
		return;
	}

	auto UpdateItems = Data.array_items();

	for (auto const& Item : UpdateItems)
	{
		auto ID = static_cast<uint64_t>(Item[JSON_KEY_ID].number_value());
		auto It = Nodes.find(ID);
		auto Type = static_cast<uint64_t>(Item[JSON_KEY_TYPE].number_value());

		if (ID == 0)
		{
			// System node
			if (Type == MT_TrafficData)
			{
				Root.Download = Item[JSON_KEY_DOWNLOAD].number_value();
				Root.Upload = Item[JSON_KEY_UPLOAD].number_value();
				continue;
			}
		}

		if (It != Nodes.end())
		{
			auto Node = It->second;

			if (Type == MT_NodeRemoved)
			{
				Node->bPendingRemoval = true;
				continue;
			}

			if (Type == MT_NodeAdded)
			{
				// Add new node
			}
			else if (Type == MT_TrafficData)
			{
				Node->Upload = Item[JSON_KEY_UPLOAD].number_value();
				Node->Download = Item[JSON_KEY_DOWNLOAD].number_value();
			}
		}
	}
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
	ImGui::TableHeadersRow();

	// recursive drawer for a node
	std::function<void(WTrafficTreeNode*, int)> DrawNode = [&](WTrafficTreeNode* Node, int Level) {
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);

		ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_SpanFullWidth;
		if (Node->Children.empty())
		{
			NodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}

		if (Level < 1)
		{
			NodeFlags |= ImGuiTreeNodeFlags_DefaultOpen;
		}

		// use the node pointer as ID so labels can repeat safely
		bool Opened = ImGui::TreeNodeEx((void*)Node, NodeFlags, "%s", Node->Name.c_str());

		if (Node->Download > 0)
		{

			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%s", WTrafficFormat::Format(Node->Download, Unit).c_str());
		}

		if (Node->Upload > 0)
		{
			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%s", WTrafficFormat::Format(Node->Upload, Unit).c_str());
		}

		ImGui::TableSetColumnIndex(4);
		// placeholder: add a DownloadLimit field to WTrafficTreeNode to show real values
		ImGui::TextDisabled("-");

		ImGui::TableSetColumnIndex(3);
		// placeholder: add a UploadLimit field to WTrafficTreeNode to show real values
		ImGui::TextDisabled("-");

		if (!(NodeFlags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
		{
			if (Opened)
			{
				for (auto& Child : Node->Children)
				{
					DrawNode(Child.get(), Level + 1);
				}
				ImGui::TreePop();
			}
		}
	};

	// draw root and its children
	DrawNode(&Root, 0);
	ImGui::EndTable();
}