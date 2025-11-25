//
// Created by usr on 22/11/2025.
//

#include "RuleWidget.hpp"
#include <imgui.h>

#include "Data/TrafficItem.hpp"
#include "Icons/IconAtlas.hpp"
#include "ClientRuleManager.hpp"
#include "EBPFCommon.h"

static constexpr ImVec2 GRuleIconSize{ 16, 16 };
static constexpr float  GIconSpacing = 4.0f;

namespace
{
	void DrawBlockOptionPopup(WRenderItemArgs const& Args, bool bIsUpload)
	{
		auto Item = Args.Item;
		auto PopupLabel =
			bIsUpload ? fmt::format("upload_popup_{}", Item->ItemId) : fmt::format("download_popup_{}", Item->ItemId);

		// Prevent moving and hide title bar; keep auto-resize for compact popup
		ImGuiWindowFlags Flags = ImGuiWindowFlags_NoMove;

		ImVec2 AnchorMin = ImGui::GetItemRectMin();
		ImVec2 AnchorMax = ImGui::GetItemRectMax();
		ImVec2 PopupPos{ (AnchorMin.x + AnchorMax.x) * 0.5f, AnchorMax.y };      // x = anchor center
		ImGui::SetNextWindowPos(PopupPos, ImGuiCond_Always, ImVec2(0.5f, 0.0f)); // pivot.x = 0.5 centers it
		if (!ImGui::BeginPopup(PopupLabel.c_str(), Flags))
		{
			return;
		}

		auto&         Rule = WClientRuleManager::GetInstance().GetOrCreateRules(Item->ItemId);
		ESwitchState& SwitchState = bIsUpload ? Rule.UploadSwitch : Rule.DownloadSwitch;

		auto DrawOption = [&](char const* IconName, char const* Tooltip, ESwitchState SetState) {
			if (WIconAtlas::GetInstance().DrawIconButton(
					fmt::format("{}_{}", IconName, Item->ItemId).c_str(), IconName, GRuleIconSize))
			{
				WTrafficItemId ParentApp = 0;
				if (Args.ParentApp)
				{
					ParentApp = Args.ParentApp->ItemId;
				}
				SwitchState = SetState;
				WClientRuleManager::SendRuleStateUpdate(Args.Item->ItemId, ParentApp, Rule);
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", Tooltip);
			}
			ImGui::SameLine();
		};

		DrawOption("placeholder", "Reset rule", SS_None);
		DrawOption(
			bIsUpload ? "uploadblock" : "downloadblock", bIsUpload ? "Block upload" : "Block download", SS_Block);
		DrawOption(
			bIsUpload ? "uploadallow" : "downloadallow", bIsUpload ? "Allow upload" : "Allow download", SS_Allow);

		ImGui::EndPopup();
	}

	void DrawLimitOptionPopup(WRenderItemArgs const& Args)
	{
		auto Item = Args.Item;
		auto PopupLabel = fmt::format("limit_popup_{}", Item->ItemId);

		// Prevent moving and hide title bar; keep auto-resize for compact popup
		ImGuiWindowFlags Flags = ImGuiWindowFlags_NoMove;

		ImVec2 AnchorMin = ImGui::GetItemRectMin();
		ImVec2 AnchorMax = ImGui::GetItemRectMax();
		ImVec2 PopupPos{ (AnchorMin.x + AnchorMax.x) * 0.5f, AnchorMax.y };      // x = anchor center
		ImGui::SetNextWindowPos(PopupPos, ImGuiCond_Always, ImVec2(0.5f, 0.0f)); // pivot.x = 0.5 centers it
		if (!ImGui::BeginPopup(PopupLabel.c_str(), Flags))
		{
			return;
		}

		//
	}
} // namespace

static bool DrawIconButton(WTrafficItemId Id, ESwitchState const& State, char const* IconName1, char const* IconName2)
{
	if (State == SS_Allow)
	{
		return WIconAtlas::GetInstance().DrawIconButton(
			fmt::format("{}_{}", Id, IconName1).c_str(), IconName1, GRuleIconSize, true);
	}
	if (State == SS_Block)
	{
		return WIconAtlas::GetInstance().DrawIconButton(
			fmt::format("{}_{}", Id, IconName2).c_str(), IconName2, GRuleIconSize, true);
	}
	return WIconAtlas::GetInstance().DrawIconButton(
		fmt::format("{}_{}_placeholder", Id, IconName1).c_str(), "placeholder", GRuleIconSize, true);
}

void WRuleWidget::Draw(WRenderItemArgs const& Args, bool)
{
	auto               Item = Args.Item;
	WTrafficItemRules* Rules{};

	// We do *not* want to create the rule here if it doesn't exist
	if (WClientRuleManager::GetInstance().HasRules(Item->ItemId))
	{
		Rules = &WClientRuleManager::GetInstance().GetOrCreateRules(Item->ItemId);
	}
	else
	{
		Rules = &EmptyDummyRules;
	}

	if (DrawIconButton(Item->ItemId, Rules->DownloadSwitch, "downloadallow", "downloadblock"))
	{
		ImGui::OpenPopup(fmt::format("download_popup_{}", Item->ItemId).c_str());
	}
	ImGui::SameLine(0.0f, GIconSpacing);
	DrawBlockOptionPopup(Args, false);

	if (DrawIconButton(Item->ItemId, Rules->UploadSwitch, "uploadallow", "uploadblock"))
	{
		ImGui::OpenPopup(fmt::format("upload_popup_{}", Item->ItemId).c_str());
	}
	DrawBlockOptionPopup(Args, true);
	ImGui::SameLine(0.0f, GIconSpacing);

	// todo limits
	WIconAtlas::GetInstance().DrawIcon("placeholder", GRuleIconSize);
	ImGui::SameLine(0.0f, GIconSpacing);
	WIconAtlas::GetInstance().DrawIcon("placeholder", GRuleIconSize);
}