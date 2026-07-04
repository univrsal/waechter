/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "DetailsWindow.hpp"

#include "imgui.h"

#include "SdlWindow.hpp"
#include "Time.hpp"
#include "AppIconAtlas.hpp"
#include "Client.hpp"
#include "Format.hpp"
#include "TrafficTree.hpp"
#include "Data/SystemItem.hpp"
#include "Icons/IconAtlas.hpp"
#include "Util/I18n.hpp"

#if !defined(__EMSCRIPTEN__)
	#include "../../Daemon/Data/IP2Asn.hpp"
	#include "Util/ProtocolDB.hpp"
#endif

static char const* SocketConnectionStateToString(ESocketConnectionState State)
{
	switch (State)
	{
		case ESocketConnectionState::Unknown:
			return TR("state.unknown");
		case ESocketConnectionState::Created:
			return TR("state.created");
		case ESocketConnectionState::Connecting:
			return TR("state.connecting");
		case ESocketConnectionState::Connected:
			return TR("state.connected");
		case ESocketConnectionState::Closed:
			return TR("state.closed");
		default:
			return TR("state.invalid");
	}
}

static char const* SocketTypeToString(uint8_t Type)
{
	switch (Type)
	{
		case ESocketType::Unknown:
			return TR("socket.unknown");
		case ESocketType::Connect:
			return TR("socket.connect");
		case ESocketType::Listen:
			return TR("socket.listen");
		case ESocketType::Listen | ESocketType::Connect:
			return TR("socket.listen_connect");
		case ESocketType::Accept:
			return TR("socket.accept");
		default:
			return TR("socket.invalid");
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
			return TR("proto.unknown");
	}
}

void WDetailsWindow::DrawFilterDetails() const
{
	auto const Filter = Tree->GetSeletedTrafficItem<WFilterItem>();

	if (Filter == nullptr)
	{
		return;
	}
	WIconAtlas::GetInstance().DrawIcon("filter", WSdlWindow::ScaleSize(ImVec2(16, 16)));

	ImGui::SameLine();
	ImGui::Text("%s", Filter->Name.c_str());
	ImGui::Separator();
	ImGui::Text("%s: %s", TR("total_download"), WStorageFormat::AutoFormat(Filter->TotalDownloadBytes).c_str());
	ImGui::Text("%s: %s", TR("total_upload"), WStorageFormat::AutoFormat(Filter->TotalUploadBytes).c_str());
	if (ImGui::Button(TR("show_system_stats")))
	{
		WStatsRequest Request{};
		WConnectionHistoryRequest HistoryRequest{};
		Request.Target = Filter->Name;
		Request.StartTime = WTime::GetEpochHours() - 60 * 60 * 24;
		Request.EndTime = WTime::GetEpochHours();
		WSdlWindow::GetInstance().GetMainWindow()->OpenStatsWindow(Request, HistoryRequest);
	}
}

void WDetailsWindow::DrawSystemDetails() const
{
	auto const System = Tree->GetSeletedTrafficItem<WSystemItem>();

	if (System == nullptr)
	{
		return;
	}

	WIconAtlas::GetInstance().DrawIcon("computer", WSdlWindow::ScaleSize(ImVec2(16, 16)));

	ImGui::SameLine();

	ImGui::Text("%s: %s", TR("hostname"), System->HostName.c_str());
	ImGui::Separator();
	ImGui::Text("%s: %s", TR("uptime"), FormattedUptime.c_str());
	ImGui::Text("%s: %s", TR("total_download"), WStorageFormat::AutoFormat(System->TotalDownloadBytes).c_str());
	ImGui::Text("%s: %s", TR("total_upload"), WStorageFormat::AutoFormat(System->TotalUploadBytes).c_str());

	if (ImGui::Button(TR("show_system_stats")))
	{
		WStatsRequest Request{};
		WConnectionHistoryRequest HistoryRequest{};
		Request.Target = "system";
		Request.StartTime = WTime::GetEpochHours() - 60 * 60 * 24;
		Request.EndTime = WTime::GetEpochHours();
		HistoryRequest.TargetName = "system";
		WSdlWindow::GetInstance().GetMainWindow()->OpenStatsWindow(Request, HistoryRequest);
	}
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
	A.DrawIconForApplication(App->ApplicationName, WSdlWindow::ScaleSize(ImVec2(16, 16)));

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
	ImGui::Text("%s: %s", TR("total_download"), WStorageFormat::AutoFormat(App->TotalDownloadBytes).c_str());
	ImGui::Text("%s: %s", TR("total_upload"), WStorageFormat::AutoFormat(App->TotalUploadBytes).c_str());

	if (ImGui::Button(TR("show_app_stats")))
	{
		WStatsRequest Request{};
		WConnectionHistoryRequest HistoryRequest{};
		Request.Target = App->ApplicationPath;
		Request.StartTime = WTime::GetEpochHours() - 60 * 60 * 24;
		Request.EndTime = WTime::GetEpochHours();
		HistoryRequest.TargetName = App->ApplicationPath;
		WSdlWindow::GetInstance().GetMainWindow()->OpenStatsWindow(Request, HistoryRequest);
	}
}

void WDetailsWindow::DrawProcessDetails() const
{
	auto const Proc = Tree->GetSeletedTrafficItem<WProcessItem>();
	if (Proc == nullptr)
	{
		return;
	}

	WIconAtlas::GetInstance().DrawIcon("process", WSdlWindow::ScaleSize(ImVec2(16, 16)));
	ImGui::SameLine();

	ImGui::Text("Process ID: %d", Proc->ProcessId);
	ImGui::Separator();
	ImGui::Text("%s: %s", TR("total_download"), WStorageFormat::AutoFormat(Proc->TotalDownloadBytes).c_str());
	ImGui::Text("%s: %s", TR("total_upload"), WStorageFormat::AutoFormat(Proc->TotalUploadBytes).c_str());
}

void WDetailsWindow::DrawSocketDetails() const
{
	auto const Sock = Tree->GetSeletedTrafficItem<WSocketItem>();
	if (Sock == nullptr)
	{
		return;
	}

	ImGui::Text("%s: %s", TR("socket_state"), SocketConnectionStateToString(Sock->ConnectionState));

	if (Sock->SocketType == ESocketType::Unknown)
	{
		ImGui::Text("%s: %s", TR("protocol"), ProtocolToString(Sock->SocketTuple.Protocol));
	}
#ifndef WDEBUG
	else
#endif
	{
		ImGui::Text("%s: %s (%i)", TR("socket_type"), SocketTypeToString(Sock->SocketType), Sock->SocketType);
	}

	if (Sock->SocketType & ESocketType::Listen && Sock->SocketTuple.Protocol == EProtocol::TCP)
	{
		ImGui::InputText(TR("details.local_endpoint"),
			const_cast<char*>(Sock->SocketTuple.LocalEndpoint.ToString().c_str()), 64, ImGuiInputTextFlags_ReadOnly);

		ImGui::Text("%s: %s", TR("protocol"), ProtocolToString(Sock->SocketTuple.Protocol));
	}
	else if (Sock->SocketType != ESocketType::Unknown)
	{

		ImGui::Separator();
		ImGui::InputText(TR("details.local_endpoint"),
			const_cast<char*>(Sock->SocketTuple.LocalEndpoint.ToString().c_str()), 64, ImGuiInputTextFlags_ReadOnly);

		bool const bIsZero = Sock->SocketTuple.RemoteEndpoint.Address.IsZero();
		if (!bIsZero)
		{
			ImGui::InputText(TR("details.remote_endpoint"),
				const_cast<char*>(Sock->SocketTuple.RemoteEndpoint.ToString().c_str()), 64,
				ImGuiInputTextFlags_ReadOnly);
		}
		if (auto Addr = Tree->ResolveAddress(Sock->SocketTuple.RemoteEndpoint.Address); !Addr.empty())
		{
			ImGui::InputText(
				TR("details.hostname"), const_cast<char*>(Addr.c_str()), 128, ImGuiInputTextFlags_ReadOnly);
		}
		ImGui::Text("%s: %s", TR("protocol"), ProtocolToString(Sock->SocketTuple.Protocol));
#if !defined(__EMSCRIPTEN__)
		auto ProtocolName = WProtocolDB::GetInstance().GetServiceName(
			Sock->SocketTuple.Protocol, Sock->SocketTuple.RemoteEndpoint.Port);
		if (ProtocolName != "unknown")
		{
			ImGui::Text("%s: %s", TR("service"), ProtocolName.c_str());
		}
#endif
		ImGui::Separator();
		ImGui::Text("%s: %s", TR("total_download"), WStorageFormat::AutoFormat(Sock->TotalDownloadBytes).c_str());
		ImGui::Text("%s: %s", TR("total_upload"), WStorageFormat::AutoFormat(Sock->TotalUploadBytes).c_str());
	}

	if (!Sock->SocketTuple.RemoteEndpoint.Address.IsZero())
	{
		if (ImGui::Button(TR("show_host_stats")))
		{
			WStatsRequest Request{};
			WConnectionHistoryRequest HistoryRequest{};
			Request.Target = Sock->SocketTuple.RemoteEndpoint.Address.ToString();
			Request.StartTime = WTime::GetEpochHours() - 60 * 60 * 24;
			Request.EndTime = WTime::GetEpochHours();
			HistoryRequest.HostTarget = std::make_shared<WIPAddress>(Sock->SocketTuple.RemoteEndpoint.Address);
			WSdlWindow::GetInstance().GetMainWindow()->OpenStatsWindow(Request, HistoryRequest);
		}
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
	bool const  bIsZero = Ep.Address.IsZero();
	if (!bIsZero)
	{
		ImGui::InputText(
			TR("remote_endpoint"), const_cast<char*>(Ep.ToString().c_str()), 64, ImGuiInputTextFlags_ReadOnly);
	}
	if (auto Addr = Tree->ResolveAddress(Ep.Address); !Addr.empty())
	{
		ImGui::InputText(TR("hostname"), const_cast<char*>(Addr.c_str()), 128, ImGuiInputTextFlags_ReadOnly);
	}

	ImGui::Separator();
	ImGui::Text("%s: %s", TR("total_download"), WStorageFormat::AutoFormat(Tuple->TotalDownloadBytes).c_str());
	ImGui::Text("%s: %s", TR("total_upload"), WStorageFormat::AutoFormat(Tuple->TotalUploadBytes).c_str());
}

WDetailsWindow::WDetailsWindow()
{
	Tree = WClient::GetInstance().GetTrafficTree();
	WTimerManager::GetInstance().AddTimer(1.0, [this] {
		WSec const UptimeSeconds = WClient::GetInstance().GetSystemUptime();
		FormattedUptime = WTime::FormatTime(UptimeSeconds);
	});
}

void WDetailsWindow::Draw()
{
	if (ImGui::Begin(TR("details.title"), nullptr, ImGuiWindowFlags_NoCollapse))
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
			case TI_Filter:
				DrawFilterDetails();
				break;
		}
	}

	auto const Sock = Tree->GetSeletedTrafficItem<WSocketItem>();
	if (Sock)
	{
		if (LookupCache.contains(Sock->SocketTuple.RemoteEndpoint.Address))
		{
			std::scoped_lock Lock(DataMutex);
			auto const&      Result = LookupCache.at(Sock->SocketTuple.RemoteEndpoint.Address);
			if (Result.ASN != 0)
			{
				WMainWindow::Get().GetFlagAtlas().DrawFlag(Result.Country, ImVec2(32, 24));
				ImGui::SameLine();
				ImGui::Separator();
				ImGui::InputText(TR("asn"), const_cast<char*>(std::format("AS{}", Result.ASN).c_str()), 64,
					ImGuiInputTextFlags_ReadOnly);
				ImGui::InputText(
					TR("country"), const_cast<char*>(Result.Country.c_str()), 64, ImGuiInputTextFlags_ReadOnly);
				ImGui::InputText(TR("organization"), const_cast<char*>(Result.Organization.c_str()), 128,
					ImGuiInputTextFlags_ReadOnly);
			}
		}
		else
		{
			std::scoped_lock Lock(DataMutex);
			// Send lookup request
			WIPLookupRequest Request;
			Request.AddressToLookup = Sock->SocketTuple.RemoteEndpoint.Address;
			WClient::GetInstance().SendMessage(MT_IPLookupRequest, Request);
			LookupCache[Sock->SocketTuple.RemoteEndpoint.Address] =
				WIP2AsnLookupResult{}; // placeholder to avoid sending multiple requests
		}
	}
	ImGui::End();
}

void WDetailsWindow::HandleLookupResult(WBuffer const& Buffer)
{
	WIP2AsnLookupResult Data{};
	if (!DeserializeMessage(Buffer, Data))
	{
		spdlog::error("Failed to deserialize lookup result");
		return;
	}
	std::scoped_lock Lock(DataMutex);
	LookupCache[Data.Address] = Data;
}
