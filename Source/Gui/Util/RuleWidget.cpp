//
// Created by usr on 22/11/2025.
//

#include "RuleWidget.hpp"
#include <imgui.h>

#include "Data/TrafficItem.hpp"
#include "Icons/IconAtlas.hpp"
#include "ClientRuleManager.hpp"
#include "EBPFCommon.h"

static constexpr ImVec2 kRuleIconSize{ 16, 16 };
static constexpr float  GIconSpacing = 4.0f;

namespace
{
	void DrawRuleOptions(std::shared_ptr<ITrafficItem> const& Item, bool bIsUpload)
	{
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

		auto&        Rule = WClientRuleManager::GetInstance().GetOrCreateRules(Item->ItemId);
		ESwitchState AllowFlag = bIsUpload ? SS_AllowUpload : SS_AllowDownload;
		ESwitchState BlockFlag = bIsUpload ? SS_DenyUpload : SS_DenyDownload;
		uint8_t&     FlagField = Rule.SwitchFlags;

		auto DrawOption = [&](char const* IconName, char const* Tooltip, ESwitchState EnableFlag,
							  ESwitchState DisableFlag) {
			if (WIconAtlas::GetInstance().DrawIconButton(
					fmt::format("{}_{}", IconName, Item->ItemId).c_str(), IconName, kRuleIconSize))
			{
				FlagField &= ~(EnableFlag | DisableFlag);
				FlagField |= EnableFlag;
				WClientRuleManager::SendRuleStateUpdate(Item->ItemId, Rule);
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", Tooltip);
			}
			ImGui::SameLine();
		};

		DrawOption("placeholder", "Reset rule", SS_None, static_cast<ESwitchState>(AllowFlag | BlockFlag));
		DrawOption(bIsUpload ? "uploadblock" : "downloadblock", bIsUpload ? "Block upload" : "Block download",
			BlockFlag, AllowFlag);
		DrawOption(bIsUpload ? "uploadallow" : "downloadallow", bIsUpload ? "Allow upload" : "Allow download",
			AllowFlag, BlockFlag);

		ImGui::EndPopup();
	}
} // namespace

static bool DrawIconButton(WTrafficItemId Id, uint8_t const& Flags, ESwitchState CheckFlag1, ESwitchState CheckFlag2,
	char const* IconName1, char const* IconName2)
{
	if (Flags & CheckFlag1)
	{
		return WIconAtlas::GetInstance().DrawIconButton(
			fmt::format("{}_{}", Id, IconName1).c_str(), IconName1, kRuleIconSize, true);
	}
	if (Flags & CheckFlag2)
	{
		return WIconAtlas::GetInstance().DrawIconButton(
			fmt::format("{}_{}", Id, IconName2).c_str(), IconName2, kRuleIconSize, true);
	}
	return WIconAtlas::GetInstance().DrawIconButton(
		fmt::format("{}_{}_placeholder", Id, IconName1).c_str(), "placeholder", kRuleIconSize, true);
}

void WRuleWidget::Draw(std::shared_ptr<ITrafficItem> const& Item, bool)
{
	WNetworkItemRules* Rules{};
	// We do *not* want to create the rule here if it doesn't exist
	if (WClientRuleManager::GetInstance().HasRules(Item->ItemId))
	{
		Rules = &WClientRuleManager::GetInstance().GetOrCreateRules(Item->ItemId);
	}
	else
	{
		Rules = &EmptyDummyRules;
	}

	if (DrawIconButton(
			Item->ItemId, Rules->SwitchFlags, SS_AllowDownload, SS_DenyDownload, "downloadallow", "downloadblock"))
	{
		ImGui::OpenPopup(fmt::format("download_popup_{}", Item->ItemId).c_str());
	}
	ImGui::SameLine(0.0f, GIconSpacing);
	DrawRuleOptions(Item, false);

	if (DrawIconButton(Item->ItemId, Rules->SwitchFlags, SS_AllowUpload, SS_DenyUpload, "uploadallow", "uploadblock"))
	{
		ImGui::OpenPopup(fmt::format("upload_popup_{}", Item->ItemId).c_str());
	}
	DrawRuleOptions(Item, true);
	ImGui::SameLine(0.0f, GIconSpacing);

	// todo limits
	WIconAtlas::GetInstance().DrawIcon("placeholder", kRuleIconSize);
	ImGui::SameLine(0.0f, GIconSpacing);
	WIconAtlas::GetInstance().DrawIcon("placeholder", kRuleIconSize);
}