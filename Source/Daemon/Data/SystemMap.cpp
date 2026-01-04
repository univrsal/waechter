/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "SystemMap.hpp"

#include <spdlog/spdlog.h>
#include <ranges>
#include <tracy/Tracy.hpp>

#include "AppIconAtlasBuilder.hpp"
#include "Filesystem.hpp"
#include "Format.hpp"
#include "NetworkEvents.hpp"
#include "Net/PacketParser.hpp"
#include "Net/Resolver.hpp"

#include <regex>

void WSystemMap::DoPacketParsing(WSocketEvent const& Event, std::shared_ptr<WSocketCounter> const& SockCounter)
{
	auto Item = SockCounter->TrafficItem;
	// if this socket doesn't already have a local/remote endpoint, try to infer from traffic
	bool const bHaveRemoteEndpoint = !Item->SocketTuple.LocalEndpoint.Address.IsZero();
	bool const bHaveLocalEndpoint = !Item->SocketTuple.RemoteEndpoint.Address.IsZero();
	bool const bIsTcp = Item->SocketTuple.Protocol == EProtocol::TCP;

	if (bHaveLocalEndpoint && bHaveRemoteEndpoint && bIsTcp)
	{
		// both endpoints are already known
		// so in the case of TCP there's nothing to do
		// udp has to always be parsed because it can send/receive from/to multiple endpoints
		return;
	}

	WPacketHeaderParser PacketHeader{};

	if (PacketHeader.ParsePacket(Event.Data.TrafficEventData.RawData, PACKET_HEADER_SIZE))
	{
		Item->SocketTuple.Protocol = PacketHeader.L4Proto;
		WEndpoint LocalEndpoint;
		WEndpoint RemoteEndpoint;

		if (Event.Data.TrafficEventData.Direction == PD_Outgoing)
		{
			LocalEndpoint = PacketHeader.Src;
			RemoteEndpoint = PacketHeader.Dst;
		}
		else
		{
			LocalEndpoint = PacketHeader.Dst;
			RemoteEndpoint = PacketHeader.Src;
		}

		{
			// ZoneScopedN("ResolveLocalEndpoint");
			// WResolver::GetInstance().Resolve(RemoteEndpoint.Address);
		}

		if (!bHaveLocalEndpoint)
		{
			Item->SocketTuple.LocalEndpoint = LocalEndpoint;
		}

		if (!bHaveRemoteEndpoint)
		{
			Item->SocketTuple.RemoteEndpoint = RemoteEndpoint;
		}

		if (Item->SocketType == ESocketType::Unknown)
		{
			if (Item->SocketTuple.Protocol == EProtocol::ICMP || Item->SocketTuple.Protocol == EProtocol::ICMPv6)
			{
				Item->SocketType = ESocketType::Connect;
			}
			else
			{
				ZoneScopedN("DetermineSocketType");
				Item->SocketType =
					SocketStateParser.DetermineSocketType(Item->SocketTuple.LocalEndpoint, Item->SocketTuple.Protocol);
			}
		}

		if (Item->SocketTuple.Protocol == EProtocol::UDP)
		{
			// Add a new UDP per-connection tuple if the remote endpoint isn't the same as the socket's main one
			if (!RemoteEndpoint.Address.IsZero())
			{
				bool bTupleExists = Item->UDPPerConnectionTraffic.contains(RemoteEndpoint);
				auto TupleCounter = GetOrCreateUDPTupleCounter(SockCounter, RemoteEndpoint);
				if (Event.Data.TrafficEventData.Direction == PD_Outgoing)
				{
					ZoneScopedN("PushOutgoingTraffic");
					TupleCounter->PushOutgoingTraffic(Event.Data.TrafficEventData.Bytes);
				}
				else
				{
					ZoneScopedN("PushIncomingTraffic");
					TupleCounter->PushIncomingTraffic(Event.Data.TrafficEventData.Bytes);
				}

				if (!bTupleExists)
				{
					MapUpdate.AddTupleAddition(RemoteEndpoint, TupleCounter);
				}
			}
		}

		MapUpdate.AddStateChange(Item->ItemId, ESocketConnectionState::Connected, Item->SocketType, Item->SocketTuple);
	}
	else
	{
		spdlog::warn("Packet header parsing failed");
	}
}

std::shared_ptr<WTupleCounter> WSystemMap::GetOrCreateUDPTupleCounter(
	std::shared_ptr<WSocketCounter> const& SockCounter, WEndpoint const& Endpoint)
{
	auto Item = SockCounter->TrafficItem;
	if (auto It = SockCounter->UDPPerConnectionCounters.find(Endpoint);
		It != SockCounter->UDPPerConnectionCounters.end())
	{
		return It->second;
	}

	auto NewItem = std::make_shared<WTupleItem>();
	NewItem->ItemId = GetNextItemId();
	Item->UDPPerConnectionTraffic[Endpoint] = NewItem;

	auto TupleCounter = std::make_shared<WTupleCounter>(NewItem, SockCounter);
	SockCounter->UDPPerConnectionCounters[Endpoint] = TupleCounter;
	return TupleCounter;
}

WSystemMap::WSystemMap()
{
	auto HostName = WFilesystem::ReadProc("/proc/sys/kernel/hostname");
	if (!HostName.empty())
	{
		SystemItem->HostName = HostName;
	}
}

std::string NormalizeAppImagePaths(std::string const& path)
{
	static std::regex const RE(R"(^/tmp/[^/]+/usr/bin/(.*)$)");
	return std::regex_replace(path, RE, "/tmp/appimage/bin/$1");
}

std::shared_ptr<WSocketCounter> WSystemMap::MapSocket(WSocketEvent const& Event, WProcessId PID, bool bSilentFail)
{
	ZoneScopedN("WSystemMap::MapSocket");
	auto SocketCookie = Event.Cookie;
	if (PID == 0)
	{
		if (!bSilentFail)
		{
			spdlog::warn("Attempted to map socket cookie {} to PID 0 for event {}", SocketCookie, Event.EventType);
		}
		return {};
	}

	if (auto It = Sockets.find(SocketCookie); It != Sockets.end())
	{
		return It->second;
	}

	// Build robust process info
	std::string              ExePath = NormalizeAppImagePaths(WFilesystem::GetProcessExePath(PID));
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

	Comm = WStringFormat::Trim(Comm);

	auto App = FindOrMapApplication(ExePath, CmdlIne, Comm);
	auto Process = FindOrMapProcess(PID, App);
	return FindOrMapSocket(SocketCookie, Process);
}

std::shared_ptr<WSocketCounter> WSystemMap::FindOrMapSocket(
	WSocketCookie SocketCookie, std::shared_ptr<WProcessCounter> const& ParentProcess)
{
	ZoneScopedN("WSystemMap::FindOrMapSocket");
	if (auto const It = Sockets.find(SocketCookie); It != Sockets.end())
	{
		return It->second;
	}

	auto SocketItem = std::make_shared<WSocketItem>();
	auto Socket = std::make_shared<WSocketCounter>(SocketItem, ParentProcess);
	SocketItem->ItemId = NextItemId++;
	SocketItem->Cookie = SocketCookie;
	Sockets[SocketCookie] = Socket;
	TrafficItems[Socket->TrafficItem->ItemId] = SocketItem;
	ParentProcess->TrafficItem->Sockets[SocketCookie] = SocketItem;

	MapUpdate.AddSocketAddition(Socket);

	return Socket;
}

std::shared_ptr<WProcessCounter> WSystemMap::FindOrMapProcess(
	WProcessId const PID, std::shared_ptr<WAppCounter> const& ParentApp)
{
	ZoneScopedN("WSystemMap::FindOrMapProcess");
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
	TrafficItems[Process->TrafficItem->ItemId] = ProcessItem;

	WNetworkEvents::GetInstance().OnProcessCreated(Process);
	return Process;
}

std::shared_ptr<WAppCounter> WSystemMap::FindOrMapApplication(
	std::string const& ExePath, std::string const& CommandLine, std::string const& AppName)
{
	ZoneScopedN("WSystemMap::FindOrMapApplication");

	// Prefer the resolved exe path as key if available; otherwise fall back to argv[0] from CommandLine's first
	// token
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
		Key = WStringFormat::Trim(Key + " " + AppName);
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
		auto Pos = Key.find_last_of('/');
		AppItem->ApplicationName = WStringFormat::Trim((Pos == std::string::npos) ? Key : Key.substr(Pos + 1));
		if (AppItem->ApplicationName.empty())
		{
			AppItem->ApplicationName = Key.empty() ? "unknown" : Key;
		}
	}

	bool bHaveCachedIcon{ false };
	auto IconPath =
		WAppIconAtlasBuilder::GetInstance().GetResolver().ResolveIcon(AppItem->ApplicationName, &bHaveCachedIcon);

	if (!IconPath.empty() && !bHaveCachedIcon)
	{
		// This is a new icon we haven't seen before, send atlas update to clients
		WAppIconAtlasBuilder::GetInstance().MarkDirty();
	}

	SystemItem->Applications[Key] = AppItem;
	Applications[Key] = App;
	TrafficItems[AppItem->ItemId] = AppItem;
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
		if (Socket->TrafficItem->SocketTuple.Protocol == EProtocol::UDP)
		{
			for (auto const& TupleCounter : Socket->UDPPerConnectionCounters | std::views::values)
			{
				TupleCounter->Refresh();
			}
		}
	}

	Cleanup();
}

void WSystemMap::PushIncomingTraffic(WSocketEvent const& Event)
{
	auto            Bytes = Event.Data.TrafficEventData.Bytes;
	auto            SocketCookie = Event.Cookie;
	std::lock_guard Lock(DataMutex);
	TrafficCounter.PushIncomingTraffic(Bytes);

	if (auto const It = Sockets.find(SocketCookie); It != Sockets.end())
	{
		auto Socket = It->second;
		{
			ZoneScopedN("WSystemMap::PushIncomingTraffic");
			Socket->PushIncomingTraffic(Bytes);
			Socket->TrafficItem->ConnectionState = ESocketConnectionState::Connected;
			Socket->ParentProcess->PushIncomingTraffic(Bytes);
			Socket->ParentProcess->ParentApp->PushIncomingTraffic(Bytes);
		}

		DoPacketParsing(Event, Socket);
	}
}

void WSystemMap::PushOutgoingTraffic(WSocketEvent const& Event)
{
	auto            Bytes = Event.Data.TrafficEventData.Bytes;
	auto            SocketCookie = Event.Cookie;
	std::lock_guard Lock(DataMutex);
	TrafficCounter.PushOutgoingTraffic(Bytes);

	if (auto const It = Sockets.find(SocketCookie); It != Sockets.end())
	{
		auto Socket = It->second;
		{
			ZoneScopedN("WSystemMap::PushOutgoingTraffic");
			Socket->PushOutgoingTraffic(Bytes);
			Socket->TrafficItem->ConnectionState = ESocketConnectionState::Connected;
			Socket->ParentProcess->PushOutgoingTraffic(Bytes);
			Socket->ParentProcess->ParentApp->PushOutgoingTraffic(Bytes);
		}

		DoPacketParsing(Event, Socket);
	}
}

std::vector<std::string> WSystemMap::GetActiveApplicationPaths()
{
	ZoneScopedN("GetActiveApplicationPaths");
	std::lock_guard          Lock(DataMutex);
	std::vector<std::string> ActiveApps{};

	for (auto const& App : Applications | std::views::values)
	{
		if (!App->TrafficItem->Processes.empty() && !App->TrafficItem->ApplicationName.empty())
		{
			ActiveApps.emplace_back(App->TrafficItem->ApplicationName);
		}
	}
	return ActiveApps;
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
			MapUpdate.MarkItemForRemoval(ProcessIt->second->TrafficItem->ItemId);
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
			//  - Remove any rules associated with the process and its sockets
			for (auto const& [SocketCookie, Socket] : Process->TrafficItem->Sockets)
			{
				WNetworkEvents::GetInstance().OnSocketRemoved(Sockets[SocketCookie]);
				Sockets.erase(SocketCookie);
				TrafficItems.erase(Socket->ItemId);
				MapUpdate.AddItemRemoval(Socket->ItemId);
			}
			WNetworkEvents::GetInstance().OnProcessRemoved(Process);
			Process->ParentApp->TrafficItem->Processes.erase(ProcessIt->first);
			MapUpdate.AddItemRemoval(ProcessIt->second->TrafficItem->ItemId);
			TrafficItems.erase(Process->TrafficItem->ItemId);
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
			WNetworkEvents::GetInstance().OnSocketRemoved(Socket);
			Socket->ParentProcess->TrafficItem->Sockets.erase(SocketIt->first);
			TrafficItems.erase(Socket->TrafficItem->ItemId);
			MapUpdate.AddItemRemoval(Socket->TrafficItem->ItemId);
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