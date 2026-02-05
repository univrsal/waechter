/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "RegisterDialog.hpp"

#include "imgui.h"
#include "imgui_internal.h"

#include "Assets.hpp"
#include "GlfwWindow.hpp"
#include "Util/I18n.hpp"
#include "Util/Settings.hpp"
#include "Util/Keylogic/validate.h"

void WRegisterDialog::Init()
{
	memcpy(Username, WSettings::GetInstance().RegisteredUsername.c_str(), sizeof(Username) - 1);
	memcpy(SerialKey, WSettings::GetInstance().RegistrationSerialKey.c_str(), sizeof(SerialKey) - 1);
	Validate();

	Watermark = WImageUtils::LoadImageFromMemoryRGBA8(GWatermarkImageData, GWatermarkImageSize);
}

void WRegisterDialog::Draw()
{
	if (!bValid)
	{
		ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y + 15),
			ImGuiCond_Always, ImVec2(1.0f, 1.0f));
		ImGui::SetNextWindowBgAlpha(0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

		if (ImGui::Begin("Watermark", nullptr,
				ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
					| ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize
					| ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration
					| ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoInputs))
		{
			ImGui::Image(Watermark.TextureId, ImVec2(Watermark.Width / 2.f, Watermark.Height / 2.f));
		}
		ImGui::End();

		// Ensure the watermark is always rendered on top of all other windows
		ImGui::BringWindowToDisplayFront(ImGui::FindWindowByName("Watermark"));

		ImGui::PopStyleVar(2);
	}
	if (!bVisible)
	{
		return;
	}

	ImGuiIO& Io = ImGui::GetIO();
	auto     DisplaySize = Io.DisplaySize;
	ImGui::SetNextWindowPos(ImVec2(DisplaySize.x * 0.5f, DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(WGlfwWindow::ScaleSize(ImVec2(300, 155)));

	if (ImGui::Begin(TR("window.register"), &bVisible,
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking))
	{
		int TextBoxFlags = ImGuiInputTextFlags_CharsNoBlank;
		ImGui::InputText(
			TR("register.username"), Username, sizeof(Username), bValid ? ImGuiInputTextFlags_ReadOnly : 0);
		ImGui::InputText(TR("register.serial_key"), SerialKey, sizeof(SerialKey),
			bValid ? (ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_Password) : TextBoxFlags);

		if (bValid)
		{
			if (ImGui::Button(TR("button.unregister")))
			{
				WSettings::GetInstance().RegisteredUsername.clear();
				WSettings::GetInstance().RegistrationSerialKey.clear();
				WSettings::GetInstance().Save();
				bValid = false;
				bFailedActivation = false;
				WGlfwWindow::GetInstance().SetTitle("Wächter (UNREGISTERED)");
			}
		}
		else
		{
			ImGui::TextLinkOpenURL(TR("about.registration"), "https://waechter.st/register");
			if (ImGui::Button(TR("button.register")))
			{
				Validate();
				bFailedActivation = true;
			}
		}

		if (bValid)
		{
			ImGui::TextColored(ImVec4(0.1f, 0.7f, 0.1f, 1.0f), "%s", TR("__activated"));
		}
		else if (bFailedActivation)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", TR("__invalid_key"));
		}
	}
	ImGui::End();
}

void WRegisterDialog::Validate()
{
	if (bValid)
		return;
	if (is_valid_key(Username, SerialKey))
	{
		WSettings::GetInstance().RegisteredUsername = Username;
		WSettings::GetInstance().RegistrationSerialKey = SerialKey;
		WSettings::GetInstance().Save();
		bValid = true;
		WGlfwWindow::GetInstance().SetTitle("Wächter");
	}
	else
	{
		WGlfwWindow::GetInstance().SetTitle("Wächter (UNREGISTERED)");
	}
}
