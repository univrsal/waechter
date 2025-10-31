//
// Created by usr on 26/10/2025.
//

#include "SystemMap.hpp"

#include <spdlog/spdlog.h>

#include "Filesystem.hpp"

#include <ranges>

WSystemMap::WSystemMap()
{
	auto HostName = WFilesystem::ReadProc("/proc/sys/kernel/hostname");
	if (!HostName.empty())
	{
		SystemItem->HostName = HostName;
	}
}

std::shared_ptr<WSystemMap::WSocketCounter> WSystemMap::MapSocket(WSocketCookie SocketCookie, WProcessId PID, bool bSilentFail)
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

	// Read the command line from /proc/[pid]/cmdline
	std::string CmdLinePath = "/proc/" + std::to_string(PID) + "/cmdline";
	std::string CommPath = "/proc/" + std::to_string(PID) + "/comm";
	auto        CmdLine = WFilesystem::ReadProc(CmdLinePath);
	auto        Comm = WFilesystem::ReadProc(CommPath);

	// Remove trailing newline from Comm if present
	if (!Comm.empty() && Comm.back() == '\n')
	{
		Comm.pop_back();
	}

	auto App = FindOrMapApplication(CmdLine, Comm);
	auto Process = FindOrMapProcess(PID, App);
	return FindOrMapSocket(SocketCookie, Process);
}

void WSystemMap::WSocketCounter::ProcessSocketEvent(WSocketEvent const& Event)
{
	// TODO: This is probably not needed
	if (Event.EventType == NE_SocketConnect_4)
	{
		TrafficItem->ConnectionState = ESocketConnectionState::Connecting;
		TrafficItem->SocketTuple.Protocol = static_cast<EProtocol::Type>(Event.Data.ConnectEventData.Protocol);
		TrafficItem->SocketTuple.RemoteEndpoint.Port = static_cast<uint16_t>(Event.Data.ConnectEventData.UserPort);
		TrafficItem->SocketTuple.RemoteEndpoint.Address.Family = EIPFamily::IPv4;
		TrafficItem->SocketTuple.RemoteEndpoint.Address.Bytes[0] = static_cast<uint8_t>(Event.Data.ConnectEventData.Addr4 >> 24 & 0xFF);
		TrafficItem->SocketTuple.RemoteEndpoint.Address.Bytes[1] = static_cast<uint8_t>(Event.Data.ConnectEventData.Addr4 >> 16 & 0xFF);
		TrafficItem->SocketTuple.RemoteEndpoint.Address.Bytes[2] = static_cast<uint8_t>(Event.Data.ConnectEventData.Addr4 >> 8 & 0xFF);
		TrafficItem->SocketTuple.RemoteEndpoint.Address.Bytes[3] = static_cast<uint8_t>(Event.Data.ConnectEventData.Addr4 & 0xFF);
	}
	else if (Event.EventType == NE_SocketConnect_6)
	{
		TrafficItem->ConnectionState = ESocketConnectionState::Connecting;
		TrafficItem->SocketTuple.Protocol = static_cast<EProtocol::Type>(Event.Data.ConnectEventData.Protocol);
		TrafficItem->SocketTuple.RemoteEndpoint.Port = static_cast<uint16_t>(Event.Data.ConnectEventData.UserPort);
		TrafficItem->SocketTuple.RemoteEndpoint.Address.Family = EIPFamily::IPv6;
		for (unsigned long i = 0; i < 4; i++)
		{
			TrafficItem->SocketTuple.RemoteEndpoint.Address.Bytes[i * 4 + 0] = static_cast<uint8_t>(Event.Data.ConnectEventData.Addr6[i] >> 24 & 0xFF);
			TrafficItem->SocketTuple.RemoteEndpoint.Address.Bytes[i * 4 + 1] = static_cast<uint8_t>(Event.Data.ConnectEventData.Addr6[i] >> 16 & 0xFF);
			TrafficItem->SocketTuple.RemoteEndpoint.Address.Bytes[i * 4 + 2] = static_cast<uint8_t>(Event.Data.ConnectEventData.Addr6[i] >> 8 & 0xFF);
			TrafficItem->SocketTuple.RemoteEndpoint.Address.Bytes[i * 4 + 3] = static_cast<uint8_t>(Event.Data.ConnectEventData.Addr6[i] & 0xFF);
		}
	}
}

std::shared_ptr<WSystemMap::WSocketCounter> WSystemMap::FindOrMapSocket(WSocketCookie SocketCookie, std::shared_ptr<WProcessCounter> const& ParentProcess)
{
	if (const auto It = Sockets.find(SocketCookie); It != Sockets.end())
	{
		return It->second;
	}

	auto SocketItem = std::make_shared<WSocketItem>();
	auto Socket = std::make_shared<WSocketCounter>(SocketItem, ParentProcess);
	SocketItem->ItemId = NextItemId++;

	Sockets[SocketCookie] = Socket;
	ParentProcess->TrafficItem->Sockets[SocketCookie] = SocketItem;

	return Socket;
}

std::shared_ptr<WSystemMap::WProcessCounter> WSystemMap::FindOrMapProcess(WProcessId const PID, std::shared_ptr<WAppCounter> const& ParentApp)
{
	if (const auto It = Processes.find(PID); It != Processes.end())
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

std::shared_ptr<WSystemMap::WAppCounter> WSystemMap::FindOrMapApplication(std::string const& AppPath, std::string const& AppName)
{
	auto It = Applications.find(AppPath);
	if (It != Applications.end())
	{
		return It->second;
	}

	spdlog::debug("Mapped new application: {}", AppPath);
	auto AppItem = std::make_shared<WApplicationItem>();
	auto App = std::make_shared<WAppCounter>(AppItem);
	AppItem->ItemId = NextItemId++;
	AppItem->ApplicationPath = AppPath;
	AppItem->ApplicationName = AppName;

	SystemItem->Applications[AppName] = AppItem;
	Applications[AppPath] = App;
	return App;
}

void WSystemMap::RefreshAllTrafficCounters()
{
	std::lock_guard Lock(DataMutex);
	TrafficCounter.Refresh();
	for (const auto& App : Applications | std::views::values)
	{
		App->Refresh();
	}

	for (const auto& Process : Processes | std::views::values)
	{
		Process->Refresh();
	}

	for (const auto& Socket : Sockets | std::views::values)
	{
		Socket->Refresh();
	}
}

void WSystemMap::PushIncomingTraffic(WBytes Bytes, WSocketCookie SocketCookie)
{
	std::lock_guard Lock(DataMutex);
	TrafficCounter.PushIncomingTraffic(Bytes);

	if (const auto It = Sockets.find(SocketCookie); It != Sockets.end())
	{
		auto Socket = It->second;
		Socket->PushIncomingTraffic(Bytes);
		Socket->ParentProcess->PushIncomingTraffic(Bytes);
		Socket->ParentProcess->ParentApp->PushIncomingTraffic(Bytes);
	}
}

void WSystemMap::PushOutgoingTraffic(WBytes Bytes, WSocketCookie SocketCookie)
{
	std::lock_guard Lock(DataMutex);
	TrafficCounter.PushOutgoingTraffic(Bytes);

	if (const auto It = Sockets.find(SocketCookie); It != Sockets.end())
	{
		auto Socket = It->second;
		Socket->PushOutgoingTraffic(Bytes);
		Socket->ParentProcess->PushOutgoingTraffic(Bytes);
		Socket->ParentProcess->ParentApp->PushOutgoingTraffic(Bytes);
	}
}

void WSystemMap::Cleanup()
{
	bool bRemovedAny{ false };

	for (auto ProcessIt = Processes.begin(); ProcessIt != Processes.end();)
	{
		auto& Process = ProcessIt->second;
		if (Process->GetState() == CS_PendingRemoval)
		{
			bRemovedAny = true;
			spdlog::info("Removing process {}.", ProcessIt->first);

			// When cleaning up a process we have to
			//  - Remove all its sockets from the Sockets map
			//  - Remove the process from its parent application's Processes map
			//  - Remove the process from the Processes map
			for (const auto& SocketCookie : Process->TrafficItem->Sockets | std::views::keys)
			{
				Sockets.erase(SocketCookie);
			}

			Process->ParentApp->TrafficItem->Processes.erase(ProcessIt->first);
			ProcessIt = Processes.erase(ProcessIt);
		}
	}

	for (auto SocketIt = Sockets.begin(); SocketIt != Sockets.end();)
	{
		auto& Socket = SocketIt->second;
		if (Socket->GetState() == CS_PendingRemoval)
		{
			bRemovedAny = true;
			spdlog::info("Removing socket {}.", SocketIt->first);

			// When cleaning up a socket we have to
			//  - Remove it from its parent process's Sockets map
			//  - Remove it from the Sockets map
			Socket->ParentProcess->TrafficItem->Sockets.erase(SocketIt->first);
			SocketIt = Sockets.erase(SocketIt);
		}
	}

	if (bRemovedAny)
	{
		auto NodeCount = Applications.size() + Processes.size() + Sockets.size();
		spdlog::debug("{} nodes remain in the system map after cleanup.", NodeCount);
	}
}