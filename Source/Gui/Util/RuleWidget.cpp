/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "RuleWidget.hpp"

#include "imgui.h"

#include "Data/TrafficItem.hpp"
#include "Icons/IconAtlas.hpp"
#include "ClientRuleManager.hpp"
#include "EBPFCommon.h"
#include "I18n.hpp"
#include "ImGuiUtil.hpp"
#include "Settings.hpp"
#include "Windows/SdlWindow.hpp"

static constexpr ImVec2 GRuleIconSize{ 16, 16 };
static constexpr float  GIconSpacing = 4.0f;
static constexpr ImVec2 GWarningPopupSize{ 260, 0 };
static WBytesPerSecond  CurrentLimit = 0;
static ETrafficUnit     SelectedUnit = TU_KiBps;

namespace
{
	void DrawBlockOptionPopup(WRenderItemArgs const& Args, bool bIsUpload)
	{
		auto Item = Args.Item;
		auto PopupLabel =
			bIsUpload ? std::format("upload_popup_{}", Item->ItemId) : std::format("download_popup_{}", Item->ItemId);

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
			if (WIconAtlas::GetInstance().DrawIconButton(std::format("{}_{}", IconName, Item->ItemId).c_str(), IconName,
					WSdlWindow::ScaleSize(GRuleIconSize)))
			{
				Rule.RuleType = ERuleType::Explicit;
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

	void DrawLimitOptionPopup(WRenderItemArgs const& Args, bool bIsUpload)
	{
		auto                Item = Args.Item;
		auto PopupLabel = std::format("{}_limit_popup_{}", bIsUpload ? "upload" : "download", Item->ItemId);

		// Prevent moving and hide title bar; keep auto-resize for compact popup
		ImGuiWindowFlags Flags;
		Flags = ImGuiWindowFlags_NoMove;

		ImVec2 AnchorMin = ImGui::GetItemRectMin();
		ImVec2 AnchorMax = ImGui::GetItemRectMax();
		ImVec2 PopupPos{ (AnchorMin.x + AnchorMax.x) * 0.5f, AnchorMax.y };      // x = anchor center
		ImGui::SetNextWindowPos(PopupPos, ImGuiCond_Always, ImVec2(0.5f, 0.0f)); // pivot.x = 0.5 centers it
		if (!ImGui::BeginPopup(PopupLabel.c_str(), Flags))
		{
			return;
		}
		ImGui::SameLine();
		auto InputFlags = ImGuiInputTextFlags_CharsDecimal;
		ImGui::SetNextItemWidth(70.0f);
		ImVec4 bg = ImGui::GetStyleColorVec4(ImGuiCol_Button);

		ImGui::PushStyleColor(ImGuiCol_FrameBg, bg);
		ImGui::InputDouble("##limit_input", &CurrentLimit, 0.0, 0.0, "%.00f", InputFlags);
		ImGui::PopStyleColor();
		ImGui::SameLine();
		auto const OldUnit = SelectedUnit;
		if (DrawUnitCombo(SelectedUnit, "##unit_combo", false))
		{
			auto const LimitBps = WTrafficFormat::ConvertToBps(CurrentLimit, OldUnit);
			CurrentLimit = WTrafficFormat::ConvertFromBps(LimitBps, SelectedUnit);
		}
		ImGui::SameLine();
		CurrentLimit = std::clamp(CurrentLimit, 0.0, 5.0 WGiB);
		if (ImGui::Button("Ok##limit_ok"))
		{
			auto& Rule = WClientRuleManager::GetInstance().GetOrCreateRules(Item->ItemId);
			Rule.RuleType = ERuleType::Explicit;
			WBytesPerSecond& LimitVal = bIsUpload ? Rule.UploadLimit : Rule.DownloadLimit;
			LimitVal = WTrafficFormat::ConvertToBps(CurrentLimit, SelectedUnit);
			WTrafficItemId ParentApp = 0;
			if (Args.ParentApp)
			{
				ParentApp = Args.ParentApp->ItemId;
			}
			WClientRuleManager::SendRuleStateUpdate(Args.Item->ItemId, ParentApp, Rule);
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
} // namespace

static bool DrawLimitIconButton(WTrafficItemId Id, WBytesPerSecond Limit, char const* IconName)
{
	if (Limit > 0)
	{
		return WIconAtlas::GetInstance().DrawIconButton(
			std::format("{}_{}", Id, IconName).c_str(), IconName, WSdlWindow::ScaleSize(GRuleIconSize), true);
	}
	return WIconAtlas::GetInstance().DrawIconButton(std::format("{}_{}_placeholder", Id, IconName).c_str(),
		"placeholder", WSdlWindow::ScaleSize(GRuleIconSize), true);
}

static bool DrawBlockIconButton(
	WTrafficItemId Id, ESwitchState const& State, char const* IconName1, char const* IconName2)
{
	if (State == SS_Allow)
	{
		return WIconAtlas::GetInstance().DrawIconButton(
			std::format("{}_{}", Id, IconName1).c_str(), IconName1, WSdlWindow::ScaleSize(GRuleIconSize), true);
	}
	if (State == SS_Block)
	{
		return WIconAtlas::GetInstance().DrawIconButton(
			std::format("{}_{}", Id, IconName2).c_str(), IconName2, WSdlWindow::ScaleSize(GRuleIconSize), true);
	}
	return WIconAtlas::GetInstance().DrawIconButton(std::format("{}_{}_placeholder", Id, IconName1).c_str(),
		"placeholder", WSdlWindow::ScaleSize(GRuleIconSize), true);
}

void WRuleWidget::DrawWarningPopup(WRenderItemArgs const& Args)
{
	auto const Item = Args.Item;
	auto const PopupLabel = std::format("warning_popup_{}", Item->ItemId);

	ImGui::SetNextWindowSize(WSdlWindow::ScaleSize(GWarningPopupSize), ImGuiCond_Appearing);
	if (!ImGui::BeginPopup(PopupLabel.c_str(), ImGuiWindowFlags_NoMove))
	{
		return;
	}

	ImGui::PushTextWrapPos(0.0f);
	ImGui::TextWrapped("%s", TR("lockout_warning_text"));
	ImGui::PopTextWrapPos();
	if (ImGui::Button(TR("generic.ok")))
	{
		ImGui::CloseCurrentPopup();
		bOpenPendingPopup = true;
	}
	ImGui::EndPopup();
}

void WRuleWidget::Draw(WRenderItemArgs const& Args, bool)
{
	auto const               Item = Args.Item;
	WTrafficItemRules const* Rules{};

	if (bOpenPendingPopup)
	{
		if (PendingPopupItemId == Item->ItemId)
		{
			ImGui::OpenPopup(PendingPopupName.c_str());
			bOpenPendingPopup = false;
			PendingPopupName.clear();
			PendingPopupItemId.reset();
		}
	}

	auto const OpenPopup = [&](std::string const& Name) {
		bool const bIsSystemItem = Args.Item->ItemId == 0;
		bool       bIsDaemonOrClientItem{};
		if (Args.Item->GetType() == TI_Application)
		{
			auto const& App = std::static_pointer_cast<WApplicationItem>(Args.Item);
			if (App->ApplicationName == "waechterd" || App->ApplicationName == "waechter")
			{
				bIsDaemonOrClientItem = true;
			}
		}
		else if (Args.Item->GetType() == TI_Process)
		{
			auto const& Process = std::static_pointer_cast<WProcessItem>(Args.Item);
			if (auto const Client = WMainWindow::GetTrafficTree()->FindClientItem())
			{
				bIsDaemonOrClientItem |= Client->Processes.contains(Process->ProcessId);
			}
			if (auto const Daemon = WMainWindow::GetTrafficTree()->FindDaemonItem())
			{
				bIsDaemonOrClientItem |= Daemon->Processes.contains(Process->ProcessId);
			}
		}
		else if (Args.Item->GetType() == TI_Socket)
		{
			auto const& Socket = std::static_pointer_cast<WSocketItem>(Args.Item);
			auto const  Cookie = Socket->Cookie;
			if (auto const Client = WMainWindow::GetTrafficTree()->FindClientItem())
			{
				for (auto const& Proc : Client->Processes | std::views::values)
				{
					bIsDaemonOrClientItem |= Proc->Sockets.contains(Cookie);
				}
			}
			if (auto const Daemon = WMainWindow::GetTrafficTree()->FindDaemonItem())
			{
				for (auto const& Proc : Daemon->Processes | std::views::values)
				{
					bIsDaemonOrClientItem |= Proc->Sockets.contains(Cookie);
				}
			}
		}

		if ((bIsSystemItem || bIsDaemonOrClientItem) && WSettings::GetInstance().SocketPath.starts_with("ws"))
		{
			// we're messing with the system item, and we are connected via a websocket
			// that means the user is pretty close to locking himself out
			ImGui::OpenPopup(std::format("warning_popup_{}", Item->ItemId).c_str());
			PendingPopupName = Name;
			PendingPopupItemId = Item->ItemId;
		}
		else
		{
			ImGui::OpenPopup(Name.c_str());
		}
	};

	// We do *not* want to create the rule here if it doesn't exist
	if (WClientRuleManager::GetInstance().HasRules(Item->ItemId))
	{
		Rules = &WClientRuleManager::GetInstance().GetOrCreateRules(Item->ItemId);
	}
	else
	{
		Rules = &EmptyDummyRules;
	}

	if (DrawBlockIconButton(Item->ItemId, Rules->DownloadSwitch, "downloadallow", "downloadblock"))
	{
		OpenPopup(std::format("download_popup_{}", Item->ItemId));
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Block/allow download");
	}
	DrawBlockOptionPopup(Args, false);
	ImGui::SameLine(0.0f, GIconSpacing);

	if (DrawBlockIconButton(Item->ItemId, Rules->UploadSwitch, "uploadallow", "uploadblock"))
	{
		OpenPopup(std::format("upload_popup_{}", Item->ItemId));
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Block/allow upload");
	}
	DrawBlockOptionPopup(Args, true);
	ImGui::SameLine(0.0f, GIconSpacing);

	auto const Rule = WClientRuleManager::GetInstance().GetOrCreateRules(Item->ItemId);
	if (DrawLimitIconButton(Item->ItemId, Rule.DownloadLimit, "downloadlimit"))
	{
		CurrentLimit = WTrafficFormat::ConvertFromBps(Rule.DownloadLimit, SelectedUnit);
		OpenPopup(std::format("download_limit_popup_{}", Item->ItemId));
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Limit download");
	}
	DrawLimitOptionPopup(Args, false);
	ImGui::SameLine(0.0f, GIconSpacing);

	if (DrawLimitIconButton(Item->ItemId, Rule.UploadLimit, "uploadlimit"))
	{
		CurrentLimit = WTrafficFormat::ConvertFromBps(Rule.UploadLimit, SelectedUnit);
		OpenPopup(std::format("upload_limit_popup_{}", Item->ItemId));
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Limit upload");
	}

	DrawLimitOptionPopup(Args, true);
	DrawWarningPopup(Args);
}