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

static char const* SocketConnectionStateToString(ESocketConnectionState State)
{
	switch (State)
	{
		case ESocketConnectionState::Unknown:
			return "Unknown";
		case ESocketConnectionState::Created:
			return "Created";
		case ESocketConnectionState::Connecting:
			return "Connecting";
		case ESocketConnectionState::Connected:
			return "Connected";
		case ESocketConnectionState::Closed:
			return "Closed";
		default:
			return "Invalid";
	}
}

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
		// Wrap long paths across lines so they don't clip outside the window
		ImGui::TextUnformatted("Path:");
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextWrapped("%s", App->ApplicationPath.c_str());
		ImGui::PopTextWrapPos();
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
	auto const* Sock = Tree->GetSeletedTrafficItem<WSocketItem>();
	if (Sock == nullptr)
	{
		return;
	}

	ImGui::Text("Socket state: %s", SocketConnectionStateToString(Sock->ConnectionState));
	ImGui::Separator();
	ImGui::Text("Local Address: %s", Sock->SocketTuple.LocalEndpoint.ToString().c_str());
	ImGui::Text("Remote Address: %s", Sock->SocketTuple.RemoteEndpoint.ToString().c_str());
	ImGui::Separator();
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