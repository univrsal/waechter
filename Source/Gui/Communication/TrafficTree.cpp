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
	{
		return;
	}

	if (ImGui::TreeNodeEx(Root.Name.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Selected | ImGuiTreeNodeFlags_DrawLinesToNodes))
	{
		ImGui::Text("Upload: %.2f B/s", Root.Upload);
		ImGui::Text("Download: %.2f B/s", Root.Download);

		for (auto& AppNode : Root.Children)
		{
			if (ImGui::TreeNodeEx(AppNode->Name.c_str(), ImGuiTreeNodeFlags_DrawLinesToNodes))
			{
				if (!AppNode->Tooltip.empty())
				{
					ImGui::TextDisabled("( %s )", AppNode->Tooltip.c_str());
				}
				ImGui::Text("Upload: %.2f B/s", AppNode->Upload);
				ImGui::Text("Download: %.2f B/s", AppNode->Download);
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}
}