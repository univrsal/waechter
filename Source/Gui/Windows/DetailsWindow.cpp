//
// Created by usr on 01/11/2025.
//

#include "DetailsWindow.hpp"

#include <imgui.h>
#include <sys/sysinfo.h>

#include "GlfwWindow.hpp"
#include "Time.hpp"
#include "AppIconAtlas.hpp"
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

static char const* SocketTypeToString(uint8_t Type)
{
	switch (Type)
	{
		case ESocketType::Unknown:
			return "Unknown";
		case ESocketType::Connect:
			return "Connect";
		case ESocketType::Listen:
			return "Listen";
		case ESocketType::Listen | ESocketType::Connect:
			return "Listen/Connect";
		case ESocketType::Accept:
			return "Accept";
		default:
			return "Invalid";
	}
}

static char const* ProtocolToString(EProtocol::Type Protocol)
{
	switch (Protocol)
	{
		case EProtocol::TCP:
			return "TCP";
		case EProtocol::UDP:
			return "UDP";
		case EProtocol::ICMP:
			return "ICMP";
		case EProtocol::ICMPv6:
			return "ICMPv6";
		default:
			return "Unknown";
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
	auto const System = Tree->GetSeletedTrafficItem<WSystemItem>();

	if (System == nullptr)
	{
		return;
	}

	WAppIconAtlas::GetInstance().DrawSystemIcon(ImVec2(16, 16));

	ImGui::SameLine();

	ImGui::Text("Hostname: %s", System->HostName.c_str());
	ImGui::Separator();
	ImGui::Text("Uptime: %s", FormattedUptime.c_str());
	ImGui::Text("Total Downloaded: %s", WStorageFormat::AutoFormat(System->TotalDownloadBytes).c_str());
	ImGui::Text("Total Uploaded: %s", WStorageFormat::AutoFormat(System->TotalUploadBytes).c_str());
}

void WDetailsWindow::DrawApplicationDetails()
{
	auto const App = Tree->GetSeletedTrafficItem<WApplicationItem>();
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
	auto const Proc = Tree->GetSeletedTrafficItem<WProcessItem>();
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
	auto const Sock = Tree->GetSeletedTrafficItem<WSocketItem>();
	if (Sock == nullptr)
	{
		return;
	}

	ImGui::Text("Socket state: %s", SocketConnectionStateToString(Sock->ConnectionState));

	if (Sock->SocketType == ESocketType::Unknown)
	{
		ImGui::Text("Protocol: %s", ProtocolToString(Sock->SocketTuple.Protocol));
	}
#ifndef WDEBUG
	else
#endif
	{
		ImGui::Text("Socket type: %s", SocketTypeToString(Sock->SocketType));
	}

	if (Sock->SocketType & ESocketType::Listen && Sock->SocketTuple.Protocol == EProtocol::TCP)
	{
		ImGui::InputText("Local Endpoint", const_cast<char*>(Sock->SocketTuple.LocalEndpoint.ToString().c_str()), 64,
			ImGuiInputTextFlags_ReadOnly);

		ImGui::Text("Protocol: %s", ProtocolToString(Sock->SocketTuple.Protocol));
	}
	else if (Sock->SocketType != ESocketType::Unknown)
	{

		ImGui::Separator();
		ImGui::InputText("Local Endpoint", const_cast<char*>(Sock->SocketTuple.LocalEndpoint.ToString().c_str()), 64,
			ImGuiInputTextFlags_ReadOnly);

		bool const bIsZero = Sock->SocketTuple.RemoteEndpoint.Address.IsZero();
		if (!bIsZero)
		{
			ImGui::InputText("Remote Endpoint", const_cast<char*>(Sock->SocketTuple.RemoteEndpoint.ToString().c_str()),
				64, ImGuiInputTextFlags_ReadOnly);
		}
		if (auto It = Tree->GetResolvedAddresses().find(Sock->SocketTuple.RemoteEndpoint.Address);
			It != Tree->GetResolvedAddresses().end())
		{
			if (!It->second.empty())
			{
				ImGui::InputText("Hostname", const_cast<char*>(It->second.c_str()), 128, ImGuiInputTextFlags_ReadOnly);
			}
		}
		ImGui::Text("Protocol: %s", ProtocolToString(Sock->SocketTuple.Protocol));
		ImGui::Separator();
		ImGui::Text("Total Downloaded: %s", WStorageFormat::AutoFormat(Sock->TotalDownloadBytes).c_str());
		ImGui::Text("Total Uploaded: %s", WStorageFormat::AutoFormat(Sock->TotalUploadBytes).c_str());

#if WAECHTER_WITH_IPQUERY_IO_API
		if (!bIsZero && WGlfwWindow::GetInstance().GetMainWindow()->GetLibCurl().IsLoaded())
		{
			IpQueryIntegration.Draw(Sock);
		}
#endif
	}
}

void WDetailsWindow::DrawTupleDetails()
{
	auto const Tuple = Tree->GetSeletedTrafficItem<WTupleItem>();
	auto const Endpoint = Tree->GetSelectedTupleEndpoint();
	if (!Tuple || !Endpoint.has_value())
	{
		return;
	}
	auto const& Ep = Endpoint.value();
	ImGui::Text("Remote Endpoint: %s", Ep.ToString().c_str());
	ImGui::Separator();
	ImGui::Text("Total Downloaded: %s", WStorageFormat::AutoFormat(Tuple->TotalDownloadBytes).c_str());
	ImGui::Text("Total Uploaded: %s", WStorageFormat::AutoFormat(Tuple->TotalUploadBytes).c_str());
}

WDetailsWindow::WDetailsWindow()
{
	Tree = WClient::GetInstance().GetTrafficTree();
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
			case TI_Tuple:
				DrawTupleDetails();
				break;
		}
	}
	ImGui::End();
}