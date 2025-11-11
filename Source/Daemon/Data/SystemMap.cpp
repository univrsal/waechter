//
// Created by usr on 26/10/2025.
//

#include "SystemMap.hpp"

#include <spdlog/spdlog.h>
#include <ranges>
#include <arpa/inet.h> // for ntohl

#include "Filesystem.hpp"

WSystemMap::WSystemMap()
{
	auto HostName = WFilesystem::ReadProc("/proc/sys/kernel/hostname");
	if (!HostName.empty())
	{
		SystemItem->HostName = HostName;
	}
}

std::shared_ptr<WSystemMap::WSocketCounter> WSystemMap::MapSocket(
	WSocketCookie SocketCookie, WProcessId PID, bool bSilentFail)
{
	if (PID == 0)
	{
		if (!bSilentFail)
		{
			spdlog::warn("{}: Attempted to map socket cookie {} to PID 0", __FUNCTION__, SocketCookie);
		}
		return {};
	}

	if (auto It = Sockets.find(SocketCookie); It != Sockets.end())
	{
		return It->second;
	}

	// Build robust process info
	std::string              ExePath = WFilesystem::GetProcessExePath(PID);
	std::vector<std::string> Argv = WFilesystem::GetProcessCmdlineArgs(PID);
	std::string              Comm = WFilesystem::ReadProc("/proc/" + std::to_string(PID) + "/comm");
	if (!Comm.empty() && Comm.back() == '\n')
	{
		Comm.pop_back();
	}

	// Fallbacks if cmdline is empty (kernel threads) or trimmed
	if (Argv.empty())
	{
		if (!ExePath.empty())
		{
			Argv.push_back(ExePath);
		}
		else if (!Comm.empty())
		{
			Argv.push_back(Comm);
		}
	}

	// If exePath missing but argv[0] exists, try to resolve to absolute via /proc/[pid]/cwd or PATH (skip PATH
	// resolution for now)
	if (ExePath.empty() && !Argv.empty())
	{
		// If argv[0] is absolute, use it; else leave empty.
		if (!Argv[0].empty() && Argv[0].front() == '/')
		{
			ExePath = Argv[0];
		}
	}

	// Reconstruct human-readable command line preserving argv boundaries with spaces
	std::string CmdlIne;
	for (size_t i = 0; i < Argv.size(); ++i)
	{
		CmdlIne += Argv[i];
		if (i + 1 < Argv.size())
			CmdlIne += ' ';
	}

	auto App = FindOrMapApplication(ExePath, CmdlIne, Comm);
	auto Process = FindOrMapProcess(PID, App);
	return FindOrMapSocket(SocketCookie, Process);
}

void WSystemMap::WSocketCounter::ProcessSocketEvent(WSocketEvent const& Event)
{
	if (Event.EventType == NE_SocketConnect_4 && TrafficItem->ConnectionState != ESocketConnectionState::Connecting)
	{
		TrafficItem->ConnectionState = ESocketConnectionState::Connecting;
		TrafficItem->SocketTuple.Protocol = static_cast<EProtocol::Type>(Event.Data.ConnectEventData.Protocol);
		TrafficItem->SocketType |= ESocketType::Connect;
		TrafficItem->SocketTuple.RemoteEndpoint.Port = static_cast<uint16_t>(Event.Data.ConnectEventData.UserPort);
		TrafficItem->SocketTuple.RemoteEndpoint.Address.FromIPv4Uint32(Event.Data.ConnectEventData.Addr4);
		GetInstance().AddStateChange(
			TrafficItem->ItemId, ESocketConnectionState::Connecting, TrafficItem->SocketType, TrafficItem->SocketTuple);
	}
	else if (Event.EventType == NE_SocketConnect_6
		&& TrafficItem->ConnectionState != ESocketConnectionState::Connecting)
	{
		TrafficItem->SocketType |= ESocketType::Connect;
		TrafficItem->ConnectionState = ESocketConnectionState::Connecting;
		TrafficItem->SocketTuple.Protocol = static_cast<EProtocol::Type>(Event.Data.ConnectEventData.Protocol);
		TrafficItem->SocketTuple.RemoteEndpoint.Port = static_cast<uint16_t>(Event.Data.ConnectEventData.UserPort);
		TrafficItem->SocketTuple.RemoteEndpoint.Address.FromIPv6Array(Event.Data.ConnectEventData.Addr6);
		GetInstance().AddStateChange(
			TrafficItem->ItemId, ESocketConnectionState::Connecting, TrafficItem->SocketType, TrafficItem->SocketTuple);
	}
	else if (Event.EventType == NE_SocketCreate)
	{
		TrafficItem->ConnectionState = ESocketConnectionState::Created;
		TrafficItem->SocketTuple.Protocol = static_cast<EProtocol::Type>(Event.Data.SocketCreateEventData.Protocol);

		if (Event.Data.SocketCreateEventData.Family == AF_INET)
		{
			TrafficItem->SocketTuple.LocalEndpoint.Address.Family = EIPFamily::IPv4;
		}
		else if (Event.Data.SocketCreateEventData.Family == AF_INET6)
		{
			TrafficItem->SocketTuple.LocalEndpoint.Address.Family = EIPFamily::IPv6;
		}
		GetInstance().AddStateChange(
			TrafficItem->ItemId, ESocketConnectionState::Created, TrafficItem->SocketType, TrafficItem->SocketTuple);
	}
	else if (Event.EventType == NE_TCPSocketEstablished_4)
	{
		// Technically we should set the state to connected here instead of constantly setting it to connected
		// in the traffic counter
		TrafficItem->SocketTuple.LocalEndpoint.Port =
			static_cast<uint16_t>(Event.Data.TCPSocketEstablishedEventData.UserPort);
		TrafficItem->SocketTuple.LocalEndpoint.Address.FromIPv4Uint32(Event.Data.TCPSocketEstablishedEventData.Addr4);
		GetInstance().AddStateChange(
			TrafficItem->ItemId, ESocketConnectionState::Connected, TrafficItem->SocketType, TrafficItem->SocketTuple);
	}
	else if (Event.EventType == NE_TCPSocketEstablished_6)
	{
		TrafficItem->SocketTuple.LocalEndpoint.Port =
			static_cast<uint16_t>(Event.Data.TCPSocketEstablishedEventData.UserPort);
		TrafficItem->SocketTuple.LocalEndpoint.Address.FromIPv6Array(Event.Data.TCPSocketEstablishedEventData.Addr6);
		GetInstance().AddStateChange(
			TrafficItem->ItemId, ESocketConnectionState::Connected, TrafficItem->SocketType, TrafficItem->SocketTuple);
	}
	else if (Event.EventType == NE_SocketBind_4)
	{
		TrafficItem->SocketTuple.LocalEndpoint.Address.FromIPv4Uint32(Event.Data.SocketBindEventData.Addr4);
		TrafficItem->SocketTuple.LocalEndpoint.Port = static_cast<uint16_t>(Event.Data.SocketBindEventData.UserPort);
		TrafficItem->SocketType |= ESocketType::Listen;
		GetInstance().AddStateChange(
			TrafficItem->ItemId, ESocketConnectionState::Connected, TrafficItem->SocketType, TrafficItem->SocketTuple);
	}
	else if (Event.EventType == NE_SocketBind_6)
	{
		TrafficItem->SocketTuple.LocalEndpoint.Address.FromIPv6Array(Event.Data.SocketBindEventData.Addr6);
		TrafficItem->SocketTuple.LocalEndpoint.Port = static_cast<uint16_t>(Event.Data.SocketBindEventData.UserPort);
		TrafficItem->SocketType |= ESocketType::Listen;
		GetInstance().AddStateChange(
			TrafficItem->ItemId, ESocketConnectionState::Connected, TrafficItem->SocketType, TrafficItem->SocketTuple);
	}
	else if (Event.EventType == NE_TCPSocketListening)
	{
		TrafficItem->ConnectionState = ESocketConnectionState::Connected;
		TrafficItem->SocketType |= ESocketType::Listen;
		GetInstance().AddStateChange(
			TrafficItem->ItemId, ESocketConnectionState::Connected, TrafficItem->SocketType, TrafficItem->SocketTuple);
	}
	else if (Event.EventType == NE_SocketAccept_4)
	{
		TrafficItem->ConnectionState = ESocketConnectionState::Connected;
		TrafficItem->SocketType = ESocketType::Accept;
		if (Event.Data.SocketAcceptEventData.Type == SOCK_STREAM)
		{
			TrafficItem->SocketTuple.Protocol = EProtocol::TCP;
		}
		else if (Event.Data.SocketAcceptEventData.Type == SOCK_DGRAM)
		{
			TrafficItem->SocketTuple.Protocol = EProtocol::UDP;
		}
		TrafficItem->SocketTuple.RemoteEndpoint.Port = Event.Data.SocketAcceptEventData.DestinationPort;
		TrafficItem->SocketTuple.RemoteEndpoint.Address.FromIPv4Uint32(
			Event.Data.SocketAcceptEventData.DestinationAddr4);
		TrafficItem->SocketTuple.LocalEndpoint.Port = Event.Data.SocketAcceptEventData.SourcePort;
		TrafficItem->SocketTuple.LocalEndpoint.Address.FromIPv4Uint32(
			Event.Data.SocketAcceptEventData.DestinationAddr4);
		GetInstance().AddStateChange(
			TrafficItem->ItemId, ESocketConnectionState::Connected, TrafficItem->SocketType, TrafficItem->SocketTuple);
	}
	else if (Event.EventType == NE_SocketAccept_6)
	{
		TrafficItem->ConnectionState = ESocketConnectionState::Connected;
		TrafficItem->SocketType = ESocketType::Accept;
		if (Event.Data.SocketAcceptEventData.Type == SOCK_STREAM)
		{
			TrafficItem->SocketTuple.Protocol = EProtocol::TCP;
		}
		else if (Event.Data.SocketAcceptEventData.Type == SOCK_DGRAM)
		{
			TrafficItem->SocketTuple.Protocol = EProtocol::UDP;
		}
		TrafficItem->SocketTuple.RemoteEndpoint.Port = Event.Data.SocketAcceptEventData.SourcePort;
		TrafficItem->SocketTuple.RemoteEndpoint.Address.FromIPv6Array(Event.Data.SocketAcceptEventData.SourceAddr6);
		TrafficItem->SocketTuple.LocalEndpoint.Port = Event.Data.SocketAcceptEventData.DestinationPort;
		TrafficItem->SocketTuple.LocalEndpoint.Address.FromIPv6Array(Event.Data.SocketAcceptEventData.DestinationAddr6);
		GetInstance().AddStateChange(
			TrafficItem->ItemId, ESocketConnectionState::Connected, TrafficItem->SocketType, TrafficItem->SocketTuple);
	}
}

std::shared_ptr<WSystemMap::WSocketCounter> WSystemMap::FindOrMapSocket(
	WSocketCookie SocketCookie, std::shared_ptr<WProcessCounter> const& ParentProcess)
{
	if (auto const It = Sockets.find(SocketCookie); It != Sockets.end())
	{
		return It->second;
	}

	auto SocketItem = std::make_shared<WSocketItem>();
	auto Socket = std::make_shared<WSocketCounter>(SocketItem, ParentProcess);
	SocketItem->ItemId = NextItemId++;

	Sockets[SocketCookie] = Socket;
	ParentProcess->TrafficItem->Sockets[SocketCookie] = SocketItem;

	AddedSockets.emplace_back(Socket);

	return Socket;
}

std::shared_ptr<WSystemMap::WProcessCounter> WSystemMap::FindOrMapProcess(
	WProcessId const PID, std::shared_ptr<WAppCounter> const& ParentApp)
{
	if (auto const It = Processes.find(PID); It != Processes.end())
	{
		return It->second;
	}

	auto ProcessItem = std::make_shared<WProcessItem>();
	auto Process = std::make_shared<WProcessCounter>(ProcessItem, ParentApp);
	ProcessItem->ItemId = NextItemId++;
	ProcessItem->ProcessId = PID;

	ParentApp->TrafficItem->Processes[PID] = ProcessItem;
	Processes[PID] = Process;
	return Process;
}

std::shared_ptr<WSystemMap::WAppCounter> WSystemMap::FindOrMapApplication(
	std::string const& ExePath, std::string const& CommandLine, std::string const& AppName)
{
	// Prefer the resolved exe path as key if available; otherwise fall back to argv[0] from CommandLine's first token
	std::string Key = ExePath;
	if (Key.empty())
	{
		Key = CommandLine;
		if (auto pos = Key.find(' '); pos != std::string::npos)
		{
			Key = Key.substr(0, pos);
		}
		// FIXME: Using just argv[0] would group all python applications under the same
		// tree entry, with the first one encountered used as the parent
		// falsely grouping python processes of unrelated programs under that application
		Key += " " + AppName;
	}

	if (auto It = Applications.find(Key); It != Applications.end())
	{
		return It->second;
	}

	spdlog::debug("Mapped new application: key='{}', exe='{}', cmd='{}'", Key, ExePath, CommandLine);
	auto AppItem = std::make_shared<WApplicationItem>();
	auto App = std::make_shared<WAppCounter>(AppItem);
	AppItem->ItemId = NextItemId++;
	AppItem->ApplicationPath = Key; // store the most reliable path we have
	AppItem->ApplicationCommandLine = CommandLine;
	// Choose display name: AppName (comm) if provided, else basename of key
	if (!AppName.empty())
	{
		AppItem->ApplicationName = AppName;
	}
	else
	{
		// derive basename
		auto pos = Key.find_last_of('/');
		AppItem->ApplicationName = (pos == std::string::npos) ? Key : Key.substr(pos + 1);
	}

	SystemItem->Applications[Key] = AppItem;
	Applications[Key] = App;
	return App;
}

void WSystemMap::RefreshAllTrafficCounters()
{
	std::lock_guard Lock(DataMutex);
	TrafficCounter.Refresh();
	for (auto const& App : Applications | std::views::values)
	{
		App->Refresh();
	}

	for (auto const& Process : Processes | std::views::values)
	{
		Process->Refresh();
	}

	for (auto const& Socket : Sockets | std::views::values)
	{
		Socket->Refresh();
	}
	Cleanup();
}

void WSystemMap::PushIncomingTraffic(WBytes Bytes, WSocketCookie SocketCookie)
{
	std::lock_guard Lock(DataMutex);
	TrafficCounter.PushIncomingTraffic(Bytes);

	if (auto const It = Sockets.find(SocketCookie); It != Sockets.end())
	{
		auto Socket = It->second;
		Socket->PushIncomingTraffic(Bytes);
		Socket->TrafficItem->ConnectionState = ESocketConnectionState::Connected;
		Socket->ParentProcess->PushIncomingTraffic(Bytes);
		Socket->ParentProcess->ParentApp->PushIncomingTraffic(Bytes);
	}
}

void WSystemMap::PushOutgoingTraffic(WBytes Bytes, WSocketCookie SocketCookie)
{
	std::lock_guard Lock(DataMutex);
	TrafficCounter.PushOutgoingTraffic(Bytes);

	if (auto const It = Sockets.find(SocketCookie); It != Sockets.end())
	{
		auto Socket = It->second;
		Socket->PushOutgoingTraffic(Bytes);
		Socket->TrafficItem->ConnectionState = ESocketConnectionState::Connected;
		Socket->ParentProcess->PushOutgoingTraffic(Bytes);
		Socket->ParentProcess->ParentApp->PushOutgoingTraffic(Bytes);
	}
}

std::vector<std::string> WSystemMap::GetActiveApplicationPaths()
{
	std::lock_guard          Lock(DataMutex);
	std::vector<std::string> ActiveApps{};

	for (auto const& [AppPath, App] : Applications)
	{
		if (!App->TrafficItem->Processes.empty())
		{
			ActiveApps.emplace_back(App->TrafficItem->ApplicationName);
		}
	}
	return ActiveApps;
}

WTrafficTreeUpdates WSystemMap::GetUpdates()
{
	WTrafficTreeUpdates Updates{};

	Updates.RemovedItems = RemovedItems;
	Updates.MarkedForRemovalItems = MarkedForRemovalItems;
	Updates.SocketStateChange = SocketStateChanges;

	if (TrafficCounter.IsActive())
	{
		Updates.UpdatedItems.emplace_back(WTrafficTreeTrafficUpdate{
			SystemItem->ItemId,
			SystemItem->DownloadSpeed,
			SystemItem->UploadSpeed,
			SystemItem->TotalDownloadBytes,
			SystemItem->TotalUploadBytes,
		});
	}

	for (auto const& App : Applications | std::views::values)
	{
		if (App->IsActive())
		{
			Updates.UpdatedItems.emplace_back(WTrafficTreeTrafficUpdate{
				App->TrafficItem->ItemId,
				App->TrafficItem->DownloadSpeed,
				App->TrafficItem->UploadSpeed,
				App->TrafficItem->TotalDownloadBytes,
				App->TrafficItem->TotalUploadBytes,
			});
		}
	}

	for (auto const& Process : Processes | std::views::values)
	{
		if (Process->IsActive())
		{
			Updates.UpdatedItems.emplace_back(WTrafficTreeTrafficUpdate{
				Process->TrafficItem->ItemId,
				Process->TrafficItem->DownloadSpeed,
				Process->TrafficItem->UploadSpeed,
				Process->TrafficItem->TotalDownloadBytes,
				Process->TrafficItem->TotalUploadBytes,
			});
		}
	}

	for (auto const& Socket : Sockets | std::views::values)
	{
		if (Socket->IsActive())
		{
			Updates.UpdatedItems.emplace_back(WTrafficTreeTrafficUpdate{
				Socket->TrafficItem->ItemId,
				Socket->TrafficItem->DownloadSpeed,
				Socket->TrafficItem->UploadSpeed,
				Socket->TrafficItem->TotalDownloadBytes,
				Socket->TrafficItem->TotalUploadBytes,
			});
		}
	}

	for (auto const& Socket : AddedSockets)
	{
		if (Socket->GetState() == CS_PendingRemoval)
		{
			// No point in sending additions for sockets that are being removed
			continue;
		}

		WTrafficTreeSocketAddition Addition{};

		auto const PPTI = Socket->ParentProcess->TrafficItem;
		auto const PATI = Socket->ParentProcess->ParentApp->TrafficItem;

		Addition.ItemId = Socket->TrafficItem->ItemId;
		Addition.ProcessItemId = PPTI->ItemId;
		Addition.ApplicationItemId = PATI->ItemId;
		Addition.ProcessId = PPTI->ProcessId;
		Addition.ApplicationPath = PATI->ApplicationPath;
		Addition.ApplicationName = PATI->ApplicationName;
		Addition.ApplicationCommandLine = PATI->ApplicationCommandLine;
		Addition.SocketTuple = Socket->TrafficItem->SocketTuple;
		Addition.ConnectionState = Socket->TrafficItem->ConnectionState;
		Updates.AddedSockets.emplace_back(Addition);
	}

	AddedSockets.clear();
	RemovedItems.clear();
	MarkedForRemovalItems.clear();
	SocketStateChanges.clear();
	return Updates;
}

void WSystemMap::Cleanup()
{
	bool bRemovedAny{ false };

	for (auto ProcessIt = Processes.begin(); ProcessIt != Processes.end();)
	{
		auto PID = ProcessIt->first;
		if (!WFilesystem::IsProcessRunning(PID) && !ProcessIt->second->IsMarkedForRemoval())
		{
			ProcessIt->second->MarkForRemoval();
			MarkedForRemovalItems.emplace_back(ProcessIt->second->TrafficItem->ItemId);
		}

		if (auto const& Process = ProcessIt->second; Process->DueForRemoval())
		{
			bRemovedAny = true;
			spdlog::info("Removing process {}.", ProcessIt->first);
			TrafficCounter.PushIncomingTraffic(0); // Force state update
			Process->ParentApp->PushIncomingTraffic(0);

			// When cleaning up a process we have to
			//  - Remove all its sockets from the Sockets map
			//  - Remove the process from its parent application's Processes map
			//  - Remove the process from the Processes map
			for (auto const& [SocketCookie, Socket] : Process->TrafficItem->Sockets)
			{
				Sockets.erase(SocketCookie);
				RemovedItems.emplace_back(Socket->ItemId);
			}

			Process->ParentApp->TrafficItem->Processes.erase(ProcessIt->first);
			RemovedItems.emplace_back(ProcessIt->second->TrafficItem->ItemId);
			ProcessIt = Processes.erase(ProcessIt);
		}
		else
		{
			++ProcessIt;
		}
	}

	for (auto SocketIt = Sockets.begin(); SocketIt != Sockets.end();)
	{
		if (auto const& Socket = SocketIt->second; Socket->DueForRemoval())
		{
			bRemovedAny = true;

			// When cleaning up a socket we have to
			//  - Remove it from its parent process's Sockets map
			//  - Remove it from the Sockets map
			Socket->ParentProcess->TrafficItem->Sockets.erase(SocketIt->first);
			RemovedItems.emplace_back(Socket->TrafficItem->ItemId);
			SocketIt = Sockets.erase(SocketIt);
		}
		else
		{
			++SocketIt;
		}
	}

	if (bRemovedAny)
	{
		auto NodeCount = Applications.size() + Processes.size() + Sockets.size();
		spdlog::debug("{} nodes remain in the system map after cleanup.", NodeCount);
	}
}