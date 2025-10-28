//
// Created by usr on 28/10/2025.
//

#include "TrafficTree.hpp"

#include <spdlog/spdlog.h>
#include <imgui.h>

#include "Json.hpp"

void WTrafficTree::LoadFromJson(std::string const& Json)
{
	std::string Err;
	WJson       TreeJson = WJson::parse(Json, Err);

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
		auto* AppNode = new WTrafficTreeNode{};
		AppNode->Name = AppName;
		AppNode->Tooltip = App[JSON_KEY_BINARY_PATH].string_value();
		AppNode->Upload = App[JSON_KEY_UPLOAD].number_value();
		AppNode->Download = App[JSON_KEY_DOWNLOAD].number_value();

		auto const& ProcsJson = App[JSON_KEY_PROCESSES].array_items();
		for (auto const& Proc : ProcsJson)
		{
			auto* ProcNode = new WTrafficTreeNode{};
			ProcNode->Name = fmt::format("PID {}", static_cast<WProcessId>(Proc[JSON_KEY_PID].number_value()));
			ProcNode->Tooltip = Proc[JSON_KEY_CMDLINE].string_value();
			ProcNode->Upload = Proc[JSON_KEY_UPLOAD].number_value();
			ProcNode->Download = Proc[JSON_KEY_DOWNLOAD].number_value();

			auto const& SocketsJson = Proc[JSON_KEY_SOCKETS].array_items();
			for (auto const& Sock : SocketsJson)
			{
				auto* SocketNode = new WTrafficTreeNode{};
				SocketNode->Name = fmt::format("Socket {}", static_cast<WSocketCookie>(Sock[JSON_KEY_SOCKET_COOKIE].number_value()));
				SocketNode->Upload = Sock[JSON_KEY_UPLOAD].number_value();
				SocketNode->Download = Sock[JSON_KEY_DOWNLOAD].number_value();
				ProcNode->Children.emplace_back(SocketNode);
			}
			AppNode->Children.emplace_back(ProcNode);
		}

		Root.Children.emplace_back(AppNode);
	}
}

void WTrafficTree::Draw()
{
	if (Root.Name.empty())
		return;

	// 5 columns: Name | upload | download | upload limit | download limit
	if (!ImGui::BeginTable("TrafficTable", 5,
			ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
	{
		return;
	}

	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("Ul.", ImGuiTableColumnFlags_WidthFixed, 100.0f);
	ImGui::TableSetupColumn("Dl.", ImGuiTableColumnFlags_WidthFixed, 100.0f);
	ImGui::TableSetupColumn("Ul. limit", ImGuiTableColumnFlags_WidthFixed, 100.0f);
	ImGui::TableSetupColumn("Dl. limit", ImGuiTableColumnFlags_WidthFixed, 100.0f);
	ImGui::TableHeadersRow();

	// recursive drawer for a node
	std::function<void(WTrafficTreeNode*)> draw_node;
	draw_node = [&](WTrafficTreeNode* node) {
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);

		ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_SpanFullWidth;
		if (node->Children.empty())
			node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

		// use the node pointer as ID so labels can repeat safely
		bool opened = ImGui::TreeNodeEx((void*)node, node_flags, "%s", node->Name.c_str());

		// other columns
		ImGui::TableSetColumnIndex(1);
		ImGui::Text("%.2f B/s", node->Upload);

		ImGui::TableSetColumnIndex(2);
		ImGui::Text("%.2f B/s", node->Download);

		ImGui::TableSetColumnIndex(3);
		// placeholder: add a UploadLimit field to WTrafficTreeNode to show real values
		ImGui::TextDisabled("-");

		ImGui::TableSetColumnIndex(4);
		// placeholder: add a DownloadLimit field to WTrafficTreeNode to show real values
		ImGui::TextDisabled("-");

		if (!(node_flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
		{
			if (opened)
			{
				for (auto& child : node->Children)
					draw_node(child.get());
				ImGui::TreePop();
			}
		}
	};

	// draw root and its children
	draw_node(&Root);
	ImGui::EndTable();
}