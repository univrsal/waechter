/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "DetailsWindow.hpp"

#include "imgui.h"

#include "GlfwWindow.hpp"
#include "Time.hpp"
#include "AppIconAtlas.hpp"
#include "Client.hpp"
#include "Format.hpp"
#include "TrafficTree.hpp"
#include "Data/SystemItem.hpp"
#include "Icons/IconAtlas.hpp"
#include "Util/I18n.hpp"

#if !defined(__EMSCRIPTEN__)
	#include "Util/IP2Asn.hpp"
	#include "Util/ProtocolDB.hpp"
#endif

static char const* SocketConnectionStateToString(ESocketConnectionState State)
{
	switch (State)
	{
		case ESocketConnectionState::Unknown:
			return TR("__state.unknown");
		case ESocketConnectionState::Created:
			return TR("__state.created");
		case ESocketConnectionState::Connecting:
			return TR("__state.connecting");
		case ESocketConnectionState::Connected:
			return TR("__state.connected");
		case ESocketConnectionState::Closed:
			return TR("__state.closed");
		default:
			return TR("__state.invalid");
	}
}

static char const* SocketTypeToString(uint8_t Type)
{
	switch (Type)
	{
		case ESocketType::Unknown:
			return TR("__socket.unknown");
		case ESocketType::Connect:
			return TR("__socket.connect");
		case ESocketType::Listen:
			return TR("__socket.listen");
		case ESocketType::Listen | ESocketType::Connect:
			return TR("__socket.listen_connect");
		case ESocketType::Accept:
			return TR("__socket.accept");
		default:
			return TR("__socket.invalid");
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
			return TR("__proto.unknown");
	}
}

void WDetailsWindow::DrawSystemDetails() const
{
	auto const System = Tree->GetSeletedTrafficItem<WSystemItem>();

	if (System == nullptr)
	{
		return;
	}

	WIconAtlas::GetInstance().DrawIcon("computer", ImVec2(16, 16));

	ImGui::SameLine();

	ImGui::Text("%s: %s", TR("__hostname"), System->HostName.c_str());
	ImGui::Separator();
	ImGui::Text("%s: %s", TR("__uptime"), FormattedUptime.c_str());
	ImGui::Text("%s: %s", TR("__total_download"), WStorageFormat::AutoFormat(System->TotalDownloadBytes).c_str());
	ImGui::Text("%s: %s", TR("__total_upload"), WStorageFormat::AutoFormat(System->TotalUploadBytes).c_str());
}

void WDetailsWindow::DrawApplicationDetails() const
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
	ImGui::Text("%s: %s", TR("__total_download"), WStorageFormat::AutoFormat(App->TotalDownloadBytes).c_str());
	ImGui::Text("%s: %s", TR("__total_upload"), WStorageFormat::AutoFormat(App->TotalUploadBytes).c_str());
}

void WDetailsWindow::DrawProcessDetails() const
{
	auto const Proc = Tree->GetSeletedTrafficItem<WProcessItem>();
	if (Proc == nullptr)
	{
		return;
	}

	WIconAtlas::GetInstance().DrawIcon("process", ImVec2(16, 16));
	ImGui::SameLine();

	ImGui::Text("Process ID : %d", Proc->ProcessId);
	ImGui::Separator();
	ImGui::Text("%s: %s", TR("__total_download"), WStorageFormat::AutoFormat(Proc->TotalDownloadBytes).c_str());
	ImGui::Text("%s: %s", TR("__total_upload"), WStorageFormat::AutoFormat(Proc->TotalUploadBytes).c_str());
}

void WDetailsWindow::DrawSocketDetails() const
{
	auto const Sock = Tree->GetSeletedTrafficItem<WSocketItem>();
	if (Sock == nullptr)
	{
		return;
	}

	ImGui::Text("%s: %s", TR("__socket_state"), SocketConnectionStateToString(Sock->ConnectionState));

	if (Sock->SocketType == ESocketType::Unknown)
	{
		ImGui::Text("%s: %s", TR("protocol"), ProtocolToString(Sock->SocketTuple.Protocol));
	}
#ifndef WDEBUG
	else
#endif
	{
		ImGui::Text("%s: %s", TR("__socket_type"), SocketTypeToString(Sock->SocketType));
	}

	if (Sock->SocketType & ESocketType::Listen && Sock->SocketTuple.Protocol == EProtocol::TCP)
	{
		ImGui::InputText(TR("local_endpoint"), const_cast<char*>(Sock->SocketTuple.LocalEndpoint.ToString().c_str()),
			64, ImGuiInputTextFlags_ReadOnly);

		ImGui::Text("%s: %s", TR("__protocol"), ProtocolToString(Sock->SocketTuple.Protocol));
	}
	else if (Sock->SocketType != ESocketType::Unknown)
	{

		ImGui::Separator();
		ImGui::InputText(TR("local_endpoint"), const_cast<char*>(Sock->SocketTuple.LocalEndpoint.ToString().c_str()),
			64, ImGuiInputTextFlags_ReadOnly);

		bool const bIsZero = Sock->SocketTuple.RemoteEndpoint.Address.IsZero();
		if (!bIsZero)
		{
			ImGui::InputText(TR("remote_endpoint"),
				const_cast<char*>(Sock->SocketTuple.RemoteEndpoint.ToString().c_str()), 64,
				ImGuiInputTextFlags_ReadOnly);
		}
		if (auto It = Tree->GetResolvedAddresses().find(Sock->SocketTuple.RemoteEndpoint.Address);
			It != Tree->GetResolvedAddresses().end())
		{
			if (!It->second.empty())
			{
				ImGui::InputText(
					TR("hostname"), const_cast<char*>(It->second.c_str()), 128, ImGuiInputTextFlags_ReadOnly);
			}
		}
		ImGui::Text("%s: %s", TR("__protocol"), ProtocolToString(Sock->SocketTuple.Protocol));
#if !defined(__EMSCRIPTEN__)
		auto ProtocolName = WProtocolDB::GetInstance().GetServiceName(
			Sock->SocketTuple.Protocol, Sock->SocketTuple.RemoteEndpoint.Port);
		if (ProtocolName != "unknown")
		{
			ImGui::Text("%s: %s", TR("__service"), ProtocolName.c_str());
		}
#endif
		ImGui::Separator();
		ImGui::Text("%s: %s", TR("__total_download"), WStorageFormat::AutoFormat(Sock->TotalDownloadBytes).c_str());
		ImGui::Text("%s: %s", TR("__total_upload"), WStorageFormat::AutoFormat(Sock->TotalUploadBytes).c_str());
	}
}

void WDetailsWindow::DrawTupleDetails() const
{
	auto const Tuple = Tree->GetSeletedTrafficItem<WTupleItem>();
	auto const Endpoint = Tree->GetSelectedTupleEndpoint();
	if (!Tuple || !Endpoint.has_value())
	{
		return;
	}
	auto const& Ep = Endpoint.value();
	ImGui::Text("%s: %s", TR("__remote_endpoint"), Ep.ToString().c_str());
	ImGui::Separator();
	ImGui::Text("%s: %s", TR("__total_download"), WStorageFormat::AutoFormat(Tuple->TotalDownloadBytes).c_str());
	ImGui::Text("%s: %s", TR("__total_upload"), WStorageFormat::AutoFormat(Tuple->TotalUploadBytes).c_str());
}

WDetailsWindow::WDetailsWindow()
{
// TODO: this should come from the daemon

	Tree = WClient::GetInstance().GetTrafficTree();
/*	WTimerManager::GetInstance().AddTimer(1.0, [this] {
		long UptimeSeconds = GetUptimeSeconds();
		FormattedUptime = WTime::FormatTime(UptimeSeconds);
	});
*/
}

void WDetailsWindow::Draw() const
{
	if (ImGui::Begin(TR("window.details"), nullptr, ImGuiWindowFlags_NoCollapse))
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

#if !defined(__EMSCRIPTEN__)
	if (WIP2Asn::GetInstance().HasDatabase())
	{
		auto const Sock = Tree->GetSeletedTrafficItem<WSocketItem>();
		if (Sock)
		{
			auto LookupResult = WIP2Asn::GetInstance().Lookup(Sock->SocketTuple.RemoteEndpoint.Address.ToString());
			if (LookupResult.has_value() && LookupResult->ASN != 0)
			{
				WMainWindow::Get().GetFlagAtlas().DrawFlag(LookupResult->Country, ImVec2(32, 24));
				ImGui::SameLine();
				ImGui::Separator();
				ImGui::InputText(TR("__asn"), const_cast<char*>(std::format("AS{}", LookupResult->ASN).c_str()), 64,
					ImGuiInputTextFlags_ReadOnly);
				ImGui::InputText(TR("__country"), const_cast<char*>(LookupResult->Country.c_str()), 64,
					ImGuiInputTextFlags_ReadOnly);
				ImGui::InputText(TR("__organization"), const_cast<char*>(LookupResult->Organization.c_str()), 128,
					ImGuiInputTextFlags_ReadOnly);
			}
		}
	}
#endif
	ImGui::End();
}
