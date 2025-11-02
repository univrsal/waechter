//
// Created by usr on 01/11/2025.
//

#include "DetailsWindow.hpp"

#include "AppIconAtlas.hpp"

#include <imgui.h>
#include <sys/sysinfo.h>

#include "GlfwWindow.hpp"
#include "Time.hpp"
#include "Data/SystemItem.hpp"

long GetUptimeSeconds()
{
	struct sysinfo SysInfo{};
	if (sysinfo(&SysInfo) != 0)
	{
		return -1;
	}
	return SysInfo.uptime;
}

void WDetailsWindow::DrawSystemDetails()
{
	auto const* System = Tree->GetSeletedTrafficItem<WSystemItem>();

	if (System == nullptr)
	{
		return;
	}

	ImGui::Text("Hostname: %s", System->HostName.c_str());
	ImGui::Separator();
	ImGui::Text("Uptime: %s", FormattedUptime.c_str());
	ImGui::Text("Total Downloaded: %s", WStorageFormat::AutoFormat(System->TotalDownloadBytes).c_str());
	ImGui::Text("Total Uploaded: %s", WStorageFormat::AutoFormat(System->TotalUploadBytes).c_str());
}

void WDetailsWindow::DrawApplicationDetails()
{
	auto const* App = Tree->GetSeletedTrafficItem<WApplicationItem>();
	if (App == nullptr)
	{
		return;
	}
	// Draw app icon
	auto&           A = WAppIconAtlas::GetInstance();
	std::lock_guard Lock(A.GetMutex());
	A.DrawIconForApplication(App->ApplicationName, ImVec2(16, 16));

	ImGui::SameLine();

	ImGui::Text("%s", App->ApplicationName.c_str());
	ImGui::Separator();
	if (!App->ApplicationPath.empty())
	{
		ImGui::Text("Path: %s", App->ApplicationPath.c_str());
	}
	ImGui::Text("Total Downloaded: %s", WStorageFormat::AutoFormat(App->TotalDownloadBytes).c_str());
	ImGui::Text("Total Uploaded: %s", WStorageFormat::AutoFormat(App->TotalUploadBytes).c_str());
}

void WDetailsWindow::DrawProcessDetails()
{
	auto const* Proc = Tree->GetSeletedTrafficItem<WProcessItem>();
	if (Proc == nullptr)
	{
		return;
	}

	WAppIconAtlas::GetInstance().DrawProcessIcon(ImVec2(16, 16));
	ImGui::SameLine();

	ImGui::Text("Process ID : %d", Proc->ProcessId);
	ImGui::Separator();
	ImGui::Text("Total Downloaded: %s", WStorageFormat::AutoFormat(Proc->TotalDownloadBytes).c_str());
	ImGui::Text("Total Uploaded: %s", WStorageFormat::AutoFormat(Proc->TotalUploadBytes).c_str());
}

void WDetailsWindow::DrawSocketDetails()
{
}

WDetailsWindow::WDetailsWindow(WTrafficTree* Tree_)
	: Tree(Tree_)
{
	WTimerManager::GetInstance().AddTimer(1.0, [this] {
		long UptimeSeconds = GetUptimeSeconds();
		FormattedUptime = WTime::FormatTime(UptimeSeconds);
	});
}

void WDetailsWindow::Draw()
{
	if (ImGui::Begin("Details", nullptr, ImGuiWindowFlags_NoCollapse))
	{
		switch (Tree->GetSelectedItemType())
		{
			case TI_System:
				DrawSystemDetails();
				break;
			case TI_Application:
				DrawApplicationDetails();
				break;
			case TI_Process:
				DrawProcessDetails();
				break;
			case TI_Socket:
				DrawSocketDetails();
				break;
		}
	}
	ImGui::End();
}