//
// Created by usr on 01/11/2025.
//

#include "DetailsWindow.hpp"

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
}

void WDetailsWindow::DrawApplicationDetails()
{
}

void WDetailsWindow::DrawProcessDetails()
{
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