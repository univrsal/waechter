/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "SystemMap.hpp"

#include <array>
#include <cstdlib>
#include <ranges>
#include <regex>
#include <utility>

#include "spdlog/spdlog.h"
#include "tracy/Tracy.hpp"

#include "AppIconAtlasBuilder.hpp"
#include "Filesystem.hpp"
#include "Format.hpp"
#include "IPLinkMsg.hpp"
#include "NetworkEvents.hpp"
#include "Db/StatsManager.hpp"
#include "Net/IPLink.hpp"
#include "Net/PacketParser.hpp"

static std::string NormalizeAppImagePaths(std::string const& path);

namespace
{
std::string CanonicalizePath(stdfs::path const& Path)
{
	std::error_code Error;
	auto            Canonical = stdfs::weakly_canonical(Path, Error);
	if (!Error)
	{
		return Canonical.string();
	}

	return Path.lexically_normal().string();
}

std::string GetBasename(std::string const& Path)
{
	auto const Pos = Path.find_last_of('/');
	return WStringFormat::Trim((Pos == std::string::npos) ? Path : Path.substr(Pos + 1));
}

std::string SanitizeProcessBinaryCandidate(std::string Value)
{
	Value = WStringFormat::Trim(Value);
	if (Value.empty())
	{
		return {};
	}

	if (auto const SpacePos = Value.find(' '); SpacePos != std::string::npos)
	{
		Value = Value.substr(0, SpacePos);
	}

	Value = WStringFormat::Trim(Value);
	while (!Value.empty() && Value.back() == ':')
	{
		Value.pop_back();
	}

	Value = WStringFormat::Trim(Value);
	if (Value.empty() || Value == "main" || Value == "Main")
	{
		return {};
	}

	if (Value.front() == '[' && Value.back() == ']')
	{
		return {};
	}

	return Value;
}

bool ProcessVisiblePathExists(stdfs::path const& LogicalPath, std::string const& ProcessRoot)
{
	if (LogicalPath.empty())
	{
		return false;
	}

	std::error_code Error;
	if (stdfs::exists(LogicalPath, Error))
	{
		return true;
	}

	if (!ProcessRoot.empty() && LogicalPath.is_absolute())
	{
		Error.clear();
		return stdfs::exists(stdfs::path(ProcessRoot) / LogicalPath.relative_path(), Error);
	}

	return false;
}

void AddUniqueEntry(std::vector<std::string>& Entries, std::string Entry)
{
	Entry = WStringFormat::Trim(Entry);
	if (Entry.empty())
	{
		return;
	}

	if (std::ranges::find(Entries, Entry) == Entries.end())
	{
		Entries.emplace_back(std::move(Entry));
	}
}

std::vector<std::string> GetExecutableSearchPaths(WProcessId PID)
{
	std::vector<std::string> Paths{};
	for (auto const& EnvVar : WFilesystem::ReadProcNulSeparated("/proc/" + std::to_string(PID) + "/environ"))
	{
		if (!WStringFormat::StartsWith(EnvVar, "PATH="))
		{
			continue;
		}

		for (auto const& PathEntry : WStringFormat::SplitString(EnvVar.substr(5), ':'))
		{
			AddUniqueEntry(Paths, PathEntry);
		}
		break;
	}

	for (auto const& PathEntry : std::array<std::string, 7>{
			 "/usr/local/sbin", "/usr/local/bin", "/usr/sbin", "/usr/bin", "/sbin", "/bin", "/snap/bin" })
	{
		AddUniqueEntry(Paths, PathEntry);
	}

	return Paths;
}

std::string ResolveProcessBinaryPath(WProcessId PID, std::vector<std::string> const& Argv, std::string const& Comm)
{
	std::string const ProcessRoot = WFilesystem::ReadLink("/proc/" + std::to_string(PID) + "/root");
	std::string const Cwd = WFilesystem::GetProcessCwd(PID);
	auto const        SearchPaths = GetExecutableSearchPaths(PID);

	std::vector<std::string> Candidates{};
	if (!Argv.empty())
	{
		AddUniqueEntry(Candidates, Argv[0]);
	}
	AddUniqueEntry(Candidates, Comm);

	for (auto const& RawCandidate : Candidates)
	{
		auto const Candidate = SanitizeProcessBinaryCandidate(RawCandidate);
		if (Candidate.empty())
		{
			continue;
		}

		if (Candidate.front() == '/')
		{
			if (ProcessVisiblePathExists(Candidate, ProcessRoot))
			{
				return NormalizeAppImagePaths(CanonicalizePath(Candidate));
			}
			continue;
		}

		if (Candidate.find('/') != std::string::npos && !Cwd.empty())
		{
			auto const ResolvedPath = stdfs::path(Cwd) / Candidate;
			if (ProcessVisiblePathExists(ResolvedPath, ProcessRoot))
			{
				return NormalizeAppImagePaths(CanonicalizePath(ResolvedPath));
			}
		}

		for (auto const& SearchPath : SearchPaths)
		{
			stdfs::path BasePath = SearchPath;
			if (!BasePath.is_absolute())
			{
				if (Cwd.empty())
				{
					continue;
				}
				BasePath = stdfs::path(Cwd) / BasePath;
			}

			auto const ResolvedPath = BasePath / Candidate;
			if (ProcessVisiblePathExists(ResolvedPath, ProcessRoot))
			{
				return NormalizeAppImagePaths(CanonicalizePath(ResolvedPath));
			}
		}
	}

	return {};
}
} // namespace

void WSystemMap::DoPacketParsing(WSocketEvent const& Event, std::shared_ptr<WSocketCounter> const& SockCounter)
{
	auto const Item = SockCounter->TrafficItem;
	// if this socket doesn't already have a local/remote endpoint, try to infer from traffic
	bool const bHaveLocalEndpoint = !Item->SocketTuple.LocalEndpoint.Address.IsZero();
	bool const bHaveRemoteEndpoint = !Item->SocketTuple.RemoteEndpoint.Address.IsZero();
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
		bool bItemModified = false;
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

		if (!bHaveLocalEndpoint)
		{
			if (Item->SocketTuple.LocalEndpoint != LocalEndpoint)
			{
				bItemModified = true;
			}
			Item->SocketTuple.LocalEndpoint = LocalEndpoint;
		}

		// Don't assign a remote endpoint to UDP sockets, the only time we do that
		// is if they explicitly connect() to an address
		if (!bHaveRemoteEndpoint && Item->SocketTuple.Protocol != EProtocol::UDP)
		{
			if (Item->SocketTuple.RemoteEndpoint != RemoteEndpoint)
			{
				bItemModified = true;
			}
			Item->SocketTuple.RemoteEndpoint = RemoteEndpoint;
		}

		if (Item->SocketType == ESocketType::Unknown)
		{
			if (Item->SocketTuple.Protocol == EProtocol::ICMP || Item->SocketTuple.Protocol == EProtocol::ICMPv6)
			{
				if (Item->SocketType != ESocketType::Connect)
				{
					bItemModified = true;
				}
				Item->SocketType = ESocketType::Connect;
			}
			else
			{
				ZoneScopedN("DetermineSocketType");
				auto const DeterminedType = SocketStateParser.DetermineSocketType(
					Item->SocketTuple.LocalEndpoint, Item->SocketTuple.Protocol, &RemoteEndpoint);

				if (Item->SocketType != DeterminedType)
				{
					bItemModified = true;
				}

				Item->SocketType = DeterminedType;
			}
		}

		if (Item->SocketTuple.Protocol == EProtocol::UDP)
		{
			// Add a new UDP per-connection tuple if the remote endpoint is different from the socket's main one
			if (!RemoteEndpoint.Address.IsZero())
			{
				bool const bTupleExists = Item->HaveUDPTuple(RemoteEndpoint);
				auto const TupleCounter = GetOrCreateUDPTupleCounter(SockCounter, RemoteEndpoint);
				if (Event.Data.TrafficEventData.Direction == PD_Outgoing)
				{
					ZoneScopedN("PushOutgoingTraffic");
					TupleCounter->PushOutgoingTraffic(Event.Data.TrafficEventData.Bytes);

					for (auto const& Filter : FilterCounters)
					{
						if (Filter->FilterFunction(TupleCounter->TrafficItem->ItemId, &LocalEndpoint, &RemoteEndpoint))
						{
							Filter->PushOutgoingTraffic(Event.Data.TrafficEventData.Bytes);
						}
					}
				}
				else
				{
					ZoneScopedN("PushIncomingTraffic");
					TupleCounter->PushIncomingTraffic(Event.Data.TrafficEventData.Bytes);

					for (auto const& Filter : FilterCounters)
					{
						if (Filter->FilterFunction(TupleCounter->TrafficItem->ItemId, &LocalEndpoint, &RemoteEndpoint))
						{
							Filter->PushIncomingTraffic(Event.Data.TrafficEventData.Bytes);
						}
					}
				}

				if (!bTupleExists)
				{
					MapUpdate.AddTupleAddition(RemoteEndpoint, TupleCounter);
				}
			}
		}
		if (bItemModified)
		{
			MapUpdate.AddStateChange(Item->ItemId, Item->ConnectionState, Item->SocketType,
				std::make_shared<WSocketTuple>(Item->SocketTuple));
		}
	}
	else
	{
		spdlog::warn("Packet header parsing failed");
	}
}

std::shared_ptr<WTupleCounter> WSystemMap::GetOrCreateUDPTupleCounter(
	std::shared_ptr<WSocketCounter> const& SockCounter, WEndpoint const& Endpoint)
{
	auto const Item = SockCounter->TrafficItem;
	if (auto const It = SockCounter->UDPPerConnectionCounters.find(Endpoint);
		It != SockCounter->UDPPerConnectionCounters.end())
	{
		return It->second;
	}

	auto NewItem = std::make_shared<WTupleItem>();
	NewItem->ItemId = GetNextItemId();
	NewItem->Endpoint = Endpoint;
	Item->UDPPerConnectionTraffic.emplace_back(NewItem);
	TrafficItems[NewItem->ItemId] = NewItem;

	auto TupleCounter = std::make_shared<WTupleCounter>(NewItem, SockCounter);
	SockCounter->UDPPerConnectionCounters[Endpoint] = TupleCounter;
	WNetworkEvents::GetInstance().OnUDPTupleCreated(TupleCounter);
	return TupleCounter;
}

void WSystemMap::RegisterDefaultFilters()
{
	auto AddFilter = [this](std::string const&                                                          Name,
						 std::function<bool(WTrafficItemId const&, WEndpoint const*, WEndpoint const*)> Func) {
		auto FilterItem = std::make_shared<WFilterItem>();
		FilterItem->Name = Name;
		FilterItem->ItemId = GetNextItemId();
		auto FilterCounter = new WFilterCounter(FilterItem);
		FilterCounter->FilterFunction = std::move(Func);
		SystemItem->Filters.emplace_back(FilterItem);
		FilterCounters.emplace_back(FilterCounter);
	};

	// idk if the checks for the local endpoint make sense here...
	AddFilter("Internet", [](WTrafficItemId const&, WEndpoint const* Local, WEndpoint const* Remote) {
		if (Remote)
		{
			return Remote->Address.IsInternetAddress();
		}
		if (Local)
		{
			return Local->Address.IsInternetAddress();
		}
		return false;
	});
	AddFilter("LAN", [](WTrafficItemId const&, WEndpoint const* Local, WEndpoint const* Remote) {
		if (Remote)
		{
			return Remote->Address.IsLANAddress();
		}
		if (Local)
		{
			return Local->Address.IsLANAddress();
		}
		return false;
	});
	AddFilter("Localhost", [](WTrafficItemId const&, WEndpoint const* Local, WEndpoint const* Remote) {
		if (Remote)
		{
			return Remote->Address.IsLocalhost();
		}
		if (Local)
		{
			return Local->Address.IsLocalhost();
		}
		return false;
	});
}

void WSystemMap::AddExistingSockets()
{
	SocketStateParser.ParseData();
	// When the daemon starts, we add any existing sockets once they send/receive data
	// this does not work for listen() sockets as they do not send/receive data, so
	// instead we add them here manually

	// Use a synthetic cookie range with the high bit set to avoid collision with real EBPF cookies
	WSocketCookie SyntheticCookie = kSyntheticCookieBase;

	auto const& ListeningSockets = SocketStateParser.GetListeningSockets();
	for (auto const& [LocalEndpoint, Protocol, PID] : ListeningSockets)
	{
		if (PID <= 0 || !WFilesystem::IsProcessRunning(PID))
		{
			continue;
		}

		WSocketEvent SyntheticEvent{};
		SyntheticEvent.Cookie = SyntheticCookie++;
		SyntheticEvent.EventType = NE_Synthetic;

		auto const Socket = MapSocket(SyntheticEvent, PID, false);
		assert(Socket);
		if (!Socket)
		{
			spdlog::error("Failed to map existing listen socket for PID {} and endpoint {}, skipping", PID,
				LocalEndpoint.ToString());
			continue;
		}
		Socket->TrafficItem->SocketTuple.LocalEndpoint = LocalEndpoint;
		Socket->TrafficItem->SocketTuple.Protocol = Protocol;
		Socket->TrafficItem->SocketType = ESocketType::Listen;
		Socket->TrafficItem->ConnectionState = ESocketConnectionState::Connected;

		spdlog::debug("Added existing listen socket: {} {} (PID {}, app '{}')", EProtocol::ToString(Protocol),
			LocalEndpoint.ToString(), PID, Socket->ParentProcess->ParentApp->TrafficItem->ApplicationName);
	}

	spdlog::info("Added {} existing listen sockets.", ListeningSockets.size());
}

void WSystemMap::ReparentOrphanedSocket(WEndpoint const& Endpoint, WProcessId NewParentProcess)
{
	std::scoped_lock Lock(DataMutex);
	if (auto const& It = OrphanedSockets.find(Endpoint); It != OrphanedSockets.end())
	{
		if (NewParentProcess == 0)
		{
			// lookup failed, so we'll just have to get rid of this socket
			OrphanedSockets.erase(It);
			return;
		}

		auto const App = It->second->ParentProcess->ParentApp;

		// Re-register the application if it was cleaned up while the socket was orphaned
		auto const& AppKey = App->TrafficItem->ApplicationPath;
		if (!Applications.contains(AppKey))
		{
			Applications[AppKey] = App;
			SystemItem->Applications[AppKey] = App->TrafficItem;
			TrafficItems[App->TrafficItem->ItemId] = App->TrafficItem;
		}

		auto const bExistingProcess = Processes.contains(NewParentProcess);
		auto const NewProcess = FindOrMapProcess(NewParentProcess, App);
		It->second->ParentProcess = NewProcess;
		NewProcess->TrafficItem->Sockets[It->second->TrafficItem->ItemId] = It->second->TrafficItem;
		Sockets[It->second->TrafficItem->Cookie] = It->second;
		spdlog::info("Reparented {} (type {}) to {}", It->second->TrafficItem->SocketTuple.ToString(),
			It->second->TrafficItem->SocketType, App->TrafficItem->ApplicationName);

		OrphanedSockets.erase(It);
		if (!bExistingProcess)
		{
			spdlog::info("New process {} created as parent for orphaned socket {}, id: {}", NewParentProcess,
				It->second->TrafficItem->SocketTuple.ToString(), It->second->TrafficItem->ItemId);
		}
		// Re-add it to the new process
		MapUpdate.AddSocketAddition(It->second);
	}
}

void WSystemMap::ReparentAcceptedSocket(std::shared_ptr<WSocketCounter> const& Socket)
{
	std::scoped_lock Lock(DataMutex);

	auto const& LocalEndpoint = Socket->TrafficItem->SocketTuple.LocalEndpoint;
	if (LocalEndpoint.Port == 0)
	{
		return;
	}

	// port_to_pid stores the master PID (whoever called bind()), but for a pre-forked
	// server like nginx the accepted fd is owned by a worker. The daemon doesn't run as
	// root so it can't read /proc/[pid]/fd to resolve the real owner itself. Delegate
	// to the IPLink process (which runs as root) using the same orphan-lookup path that
	// fork() reparenting already uses.
	spdlog::debug("ReparentAcceptedSocket: queuing IPLink lookup for accepted socket on {}", LocalEndpoint.ToString());

	OrphanedSockets[LocalEndpoint] = Socket;

	WLookupEndpointsMsg LookupMsg{};
	LookupMsg.Endpoints.emplace_back(LocalEndpoint);
	WIPLink::GetInstance().SendLookupMessage(LookupMsg);
}

WSystemMap::WSystemMap()
{
	auto const HostName = WFilesystem::ReadProc("/proc/sys/kernel/hostname");
	if (!HostName.empty())
	{
		SystemItem->HostName = HostName;
	}
	RegisterDefaultFilters();
	// Periodically check /proc/ for any socket info (I don't think we need this)
	// WTimerManager::GetInstance().AddTimer(5, [this] {
	// 	std::scoped_lock Lock(DataMutex);
	// 	SocketStateParser.ParseData();
	// });
}

static std::string NormalizeAppImagePaths(std::string const& path)
{
	static std::regex const RE(R"(^/tmp/[^/]+/usr/bin/(.*)$)");
	return std::regex_replace(path, RE, "/tmp/appimage/bin/$1");
}

std::shared_ptr<WSocketCounter> WSystemMap::MapSocket(WSocketEvent const& Event, WProcessId PID, bool const bSilentFail)
{
	std::scoped_lock Lock(DataMutex);

	ZoneScopedN("WSystemMap::MapSocket");
	auto SocketCookie = Event.Cookie;
	if (SocketCookie == 0)
	{
		spdlog::warn("Received socket event with invalid cookie 0 for PID {} and event type {}", PID, Event.EventType);
		return {};
	}

	if (PID == 0)
	{
		if (!bSilentFail)
		{
			spdlog::warn("Attempted to map socket cookie {} to PID 0 for event {}", SocketCookie, Event.EventType);
		}
		else if (Event.EventType == NE_TCPSocketEstablished_4 || Event.EventType == NE_TCPSocketEstablished_6)
		{
			spdlog::warn(
				"[MapSocket] TCPSocketEstablished event for cookie {} has PID 0 — port_to_pid lookup likely failed",
				SocketCookie);
		}
		return {};
	}

	if (auto const It = Sockets.find(SocketCookie); It != Sockets.end())
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

	// If /proc/[pid]/exe is inaccessible (common for setproctitle()-style daemons such as nginx/php-fpm),
	// fall back to argv[0], comm, PATH, cwd, and the process root.
	if (ExePath.empty())
	{
		ExePath = ResolveProcessBinaryPath(PID, Argv, Comm);
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

	if ((Comm.empty() || Comm == "main" || Comm == "Main") && !ExePath.empty())
	{
		Comm = GetBasename(ExePath);
		if (Comm.empty())
		{
			Comm = ExePath.empty() ? "unknown" : ExePath;
		}
	}

	auto const App = FindOrMapApplication(ExePath, CmdlIne, Comm);
	assert(App);
	auto const Process = FindOrMapProcess(PID, App);
	assert(Process);
	return FindOrMapSocket(SocketCookie, Process);
}

std::shared_ptr<WSocketCounter> WSystemMap::MapSocketFromTrafficEvent(WSocketEvent const& Event)
{
	ZoneScopedN("WSystemMap::MapSocketFromTrafficEvent");

	WPacketHeaderParser PacketHeader{};
	if (!PacketHeader.ParsePacket(Event.Data.TrafficEventData.RawData, PACKET_HEADER_SIZE))
	{
		spdlog::warn("Failed to map unknown socket {} from traffic: packet header parsing failed", Event.Cookie);
		return {};
	}

	auto const LocalEndpoint =
		Event.Data.TrafficEventData.Direction == PD_Outgoing ? PacketHeader.Src : PacketHeader.Dst;

	auto const PID = SocketStateParser.GetEndpointPID(LocalEndpoint);
	if (PID <= 0)
	{
		spdlog::trace("Failed to map unknown socket {} from traffic: no PID found for local endpoint {}", Event.Cookie,
			LocalEndpoint.ToString());
		return {};
	}

	auto Socket = MapSocket(Event, PID, false);
	if (Socket)
	{
		if (Socket->TrafficItem->SocketTuple.LocalEndpoint.Address.IsZero()
			|| Socket->TrafficItem->SocketTuple.LocalEndpoint.Port == 0)
		{
			Socket->TrafficItem->SocketTuple.LocalEndpoint = LocalEndpoint;
		}

		if (Socket->TrafficItem->SocketTuple.Protocol == EProtocol::Unknown)
		{
			Socket->TrafficItem->SocketTuple.Protocol = PacketHeader.L4Proto;
		}

		spdlog::debug(
			"Mapped unknown socket {} from traffic to PID {} via {}", Event.Cookie, PID, LocalEndpoint.ToString());
	}

	return Socket;
}

void WSystemMap::PushTrafficForSocket(WSocketEvent const& Event, std::shared_ptr<WSocketCounter> const& Socket) const
{
	auto const Bytes = Event.Data.TrafficEventData.Bytes;
	if (Event.Data.TrafficEventData.Direction == PD_Incoming)
	{
		ZoneScopedN("WSystemMap::PushIncomingTraffic");
		Socket->PushIncomingTraffic(Bytes);
		Socket->ParentProcess->PushIncomingTraffic(Bytes);
		Socket->ParentProcess->ParentApp->PushIncomingTraffic(Bytes);

		for (auto const& Filter : FilterCounters)
		{
			if (Filter->FilterFunction(Socket->TrafficItem->ItemId, &Socket->TrafficItem->SocketTuple.LocalEndpoint,
					&Socket->TrafficItem->SocketTuple.RemoteEndpoint))
			{
				Filter->PushIncomingTraffic(Bytes);
			}
		}
	}
	else
	{
		ZoneScopedN("WSystemMap::PushOutgoingTraffic");
		Socket->PushOutgoingTraffic(Bytes);
		Socket->ParentProcess->PushOutgoingTraffic(Bytes);
		Socket->ParentProcess->ParentApp->PushOutgoingTraffic(Bytes);

		for (auto const& Filter : FilterCounters)
		{
			if (Filter->FilterFunction(Socket->TrafficItem->ItemId, &Socket->TrafficItem->SocketTuple.LocalEndpoint,
					&Socket->TrafficItem->SocketTuple.RemoteEndpoint))
			{
				Filter->PushOutgoingTraffic(Bytes);
			}
		}
	}

	Socket->TrafficItem->ConnectionState = ESocketConnectionState::Connected;
}

std::shared_ptr<WSocketCounter> WSystemMap::FindOrMapSocket(
	WSocketCookie const SocketCookie, std::shared_ptr<WProcessCounter> const& ParentProcess)
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

void WSystemMap::MergeSyntheticSocket(std::shared_ptr<WSocketCounter> const& Socket)
{
	auto const& Cookie = Socket->TrafficItem->Cookie;
	// Only merge real eBPF cookies (skip synthetic and traffic-mapped cookies)
	if ((Cookie & kSyntheticCookieBase) != 0)
	{
		return;
	}
	auto const& ParentProcess = Socket->ParentProcess;
	auto const  Port = Socket->TrafficItem->SocketTuple.LocalEndpoint.Port;
	if (Port == 0 || !ParentProcess)
	{
		return;
	}

	std::scoped_lock Lock(DataMutex);
	// Find a synthetic entry (AddExistingSockets) with the same port and parent process
	for (auto const& [ExistingCookie, ExistingSocket] : Sockets)
	{
		if ((ExistingCookie & kSyntheticCookieBase) == 0)
		{
			continue;
		}
		if (ExistingSocket->ParentProcess != ParentProcess)
		{
			continue;
		}
		if (ExistingSocket->TrafficItem->SocketTuple.LocalEndpoint.Port != Port)
		{
			continue;
		}
		// Found a synthetic duplicate - keep the correct endpoint from /proc/net/
		// and re-key the entry to the real eBPF cookie.
		auto const CorrectAddr = ExistingSocket->TrafficItem->SocketTuple.LocalEndpoint.Address;
		auto const CorrectProto = ExistingSocket->TrafficItem->SocketTuple.Protocol;

		spdlog::debug("Merging synthetic socket cookie {:#x} ({}:{}) into real cookie {:#x}", ExistingCookie,
			CorrectAddr.ToString(), Port, Cookie);

		// Transfer the correct endpoint from the synthetic entry to the real one
		Socket->TrafficItem->SocketTuple.LocalEndpoint.Address = CorrectAddr;
		if (Socket->TrafficItem->SocketTuple.Protocol == EProtocol::Unknown)
		{
			Socket->TrafficItem->SocketTuple.Protocol = CorrectProto;
		}
		// Remove the now-redundant synthetic entry
		WNetworkEvents::GetInstance().OnSocketRemoved(ExistingSocket);
		ParentProcess->TrafficItem->Sockets.erase(ExistingCookie);
		TrafficItems.erase(ExistingSocket->TrafficItem->ItemId);
		Sockets.erase(ExistingCookie);
		MapUpdate.MarkItemForRemoval(ExistingSocket->TrafficItem->ItemId);
		break;
	}
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

	// Prefer the resolved exe path as key if available; otherwise fall back to a sanitized process title.
	std::string Key = ExePath;
	if (Key.empty())
	{
		Key = CommandLine;
	}
	if (Key.empty())
	{
		Key = AppName;
	}

	if (auto const Pos = Key.find(' '); Pos != std::string::npos)
	{
		Key = Key.substr(0, Pos);
	}

	Key = WStringFormat::Trim(Key);
	if (Key.empty() || Key.front() != '/')
	{
		Key = SanitizeProcessBinaryCandidate(Key);
	}
	Key = NormalizeAppImagePaths(Key);
	if (Key.empty())
	{
		Key = AppName.empty() ? "unknown" : WStringFormat::Trim(AppName);
	}

	if (auto const It = Applications.find(Key); It != Applications.end())
	{
		return It->second;
	}

	if (!Key.empty() && Key.front() == '/')
	{
		auto const KeyBasename = GetBasename(Key);
		auto const AppAlias = SanitizeProcessBinaryCandidate(AppName);
		for (auto It = Applications.begin(); It != Applications.end(); ++It)
		{
			auto const ExistingKey = It->first;
			auto const ExistingApp = It->second;
			auto const& ExistingPath = ExistingApp->TrafficItem->ApplicationPath;
			if (!ExistingPath.empty() && ExistingPath.front() == '/')
			{
				continue;
			}

			auto ExistingAlias = SanitizeProcessBinaryCandidate(ExistingPath);
			if (ExistingAlias.empty())
			{
				ExistingAlias = SanitizeProcessBinaryCandidate(ExistingKey);
			}

			bool const bMatchesAlias = !ExistingAlias.empty() && (ExistingAlias == KeyBasename || ExistingAlias == AppAlias);
			bool const bMatchesName = !AppName.empty() && ExistingApp->TrafficItem->ApplicationName == AppName;
			if (!bMatchesAlias && !bMatchesName)
			{
				continue;
			}

			spdlog::debug("Upgrading application key '{}' -> '{}'", ExistingKey, Key);
			SystemItem->Applications.erase(ExistingKey);
			Applications.erase(It);
			ExistingApp->TrafficItem->ApplicationPath = Key;
			if (!CommandLine.empty())
			{
				ExistingApp->TrafficItem->ApplicationCommandLine = CommandLine;
			}
			if (!AppName.empty())
			{
				ExistingApp->TrafficItem->ApplicationName = AppName;
			}
			SystemItem->Applications[Key] = ExistingApp->TrafficItem;
			Applications[Key] = ExistingApp;
			return ExistingApp;
		}
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
	auto const IconPath =
		WAppIconAtlasBuilder::GetInstance().GetResolver().ResolveIcon(AppItem->ApplicationName, &bHaveCachedIcon);

	if (!IconPath.empty() && !bHaveCachedIcon)
	{
		// This is a new icon we haven't seen before, send atlas update to clients
		WAppIconAtlasBuilder::GetInstance().MarkDirty();
	}

	SystemItem->Applications[Key] = AppItem;
	Applications[Key] = App;
	TrafficItems[AppItem->ItemId] = AppItem;
	WNetworkEvents::GetInstance().OnAppFirstTimeConnected(App);
	return App;
}

void WSystemMap::RefreshAllTrafficCounters()
{
	std::lock_guard Lock(DataMutex);
	TrafficCounter.Refresh();

	WStatsManager::GetInstance().GetDataMutex().lock();
	for (auto const& Filter : FilterCounters)
	{
		WStatsManager::GetInstance().UpdateFilterStats(
			Filter->TrafficItem->Name, Filter->GetRecentDownload(), Filter->GetRecentUpload());
		Filter->Refresh();
	}
	WStatsManager::GetInstance().GetDataMutex().unlock();

	for (auto const& App : Applications | std::views::values)
	{
		App->Refresh();
	}

	for (auto const& Process : Processes | std::views::values)
	{
		Process->Refresh();
	}

	WStatsManager::GetInstance().GetDataMutex().lock();
	for (auto const& Socket : Sockets | std::views::values)
	{
		if (Socket->ParentProcess && Socket->ParentProcess->ParentApp)
		{
			auto const App = Socket->ParentProcess->ParentApp;
			WStatsManager::GetInstance().UpdateAppStats(App->TrafficItem->ItemId,
				App->TrafficItem->ApplicationPath, Socket->TrafficItem->SocketTuple.RemoteEndpoint.Address,
				Socket->GetRecentDownload(),
				Socket->GetRecentUpload());
		}

		Socket->Refresh();
		for (auto const& TupleCounter : Socket->UDPPerConnectionCounters | std::views::values)
		{
			TupleCounter->Refresh();
		}
	}
	WStatsManager::GetInstance().GetDataMutex().unlock();

	Cleanup();
}

void WSystemMap::PushIncomingTraffic(WSocketEvent const& Event)
{
	auto const       Bytes = Event.Data.TrafficEventData.Bytes;
	auto const       SocketCookie = Event.Cookie;
	std::unique_lock Lock(DataMutex);
	TrafficCounter.PushIncomingTraffic(Bytes);

	std::shared_ptr<WSocketCounter> Socket{};
	if (auto const It = Sockets.find(SocketCookie); It != Sockets.end())
	{
		Socket = It->second;
	}
	else
	{
		return;
	}

	PushTrafficForSocket(Event, Socket);
	DoPacketParsing(Event, Socket);
}

void WSystemMap::PushOutgoingTraffic(WSocketEvent const& Event)
{
	auto const       Bytes = Event.Data.TrafficEventData.Bytes;
	auto             SocketCookie = Event.Cookie;
	std::unique_lock Lock(DataMutex);
	TrafficCounter.PushOutgoingTraffic(Bytes);

	std::shared_ptr<WSocketCounter> Socket{};
	if (auto const It = Sockets.find(SocketCookie); It != Sockets.end())
	{
		Socket = It->second;
	}
	else
	{
		Lock.unlock();
		Socket = MapSocketFromTrafficEvent(Event);
		Lock.lock();

		if (!Socket)
		{
			return;
		}
	}

	PushTrafficForSocket(Event, Socket);
	DoPacketParsing(Event, Socket);
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

WMemoryStat WSystemMap::GetMemoryUsage()
{
	std::scoped_lock Lock(DataMutex);
	WMemoryStat      Stats;
	Stats.Name = "WSystemMap";
	WMemoryStatEntry Apps{}, ProcessesEntry{}, SocketsEntry{}, TrafficItemsEntry{}, UDPPerConnectionCountersEntry{},
		FiltersEntry{};

	Apps.Name = "Applications";
	Apps.Usage += sizeof(decltype(Applications));
	for (auto const& AppPath : Applications | std::views::keys)
	{
		Apps.Usage += AppPath.capacity();
		Apps.Usage += sizeof(WAppCounter);
		Apps.Usage += sizeof(WApplicationItem);
	}

	ProcessesEntry.Name = "Processes";
	ProcessesEntry.Usage += sizeof(decltype(Processes));
	ProcessesEntry.Usage += (sizeof(WProcessId) + sizeof(WProcessCounter) + sizeof(WProcessItem)) * Processes.size();

	SocketsEntry.Name = "Sockets";
	SocketsEntry.Usage += sizeof(decltype(Sockets));
	SocketsEntry.Usage += (sizeof(WSocketCookie) + sizeof(WSocketCounter) + sizeof(WSocketItem)) * Sockets.size();

	UDPPerConnectionCountersEntry.Name = "UDP connections";
	for (auto const& Socket : Sockets | std::views::values)
	{
		UDPPerConnectionCountersEntry.Usage +=
			Socket->UDPPerConnectionCounters.size() * (sizeof(WEndpoint) + sizeof(WTupleCounter) + sizeof(WTupleItem));
	}

	FiltersEntry.Name = "Filters";
	FiltersEntry.Usage = sizeof(decltype(FilterCounters));
	FiltersEntry.Usage += sizeof(WFilterCounter) * FilterCounters.size();
	FiltersEntry.Usage += sizeof(WFilterItem) * FilterCounters.size();

	TrafficItemsEntry.Name = "All traffic items";
	TrafficItemsEntry.Usage += sizeof(decltype(TrafficItems));
	TrafficItemsEntry.Usage += (sizeof(WTrafficItemId) + sizeof(ITrafficItem)) * TrafficItems.size();

	Stats.ChildEntries.emplace_back(Apps);
	Stats.ChildEntries.emplace_back(ProcessesEntry);
	Stats.ChildEntries.emplace_back(SocketsEntry);
	Stats.ChildEntries.emplace_back(TrafficItemsEntry);
	Stats.ChildEntries.emplace_back(UDPPerConnectionCountersEntry);
	Stats.ChildEntries.emplace_back(FiltersEntry);
	return Stats;
}

void WSystemMap::Cleanup()
{
	bool bRemovedAny{ false };

	auto OldSocketCount = Sockets.size();
	auto OldProcessCount = Processes.size();
	auto OldTrafficItemCount = TrafficItems.size();

	for (auto ProcessIt = Processes.begin(); ProcessIt != Processes.end();)
	{
		auto const PID = ProcessIt->first;
		if (!WFilesystem::IsProcessRunning(PID) && !ProcessIt->second->IsMarkedForRemoval())
		{
			ProcessIt->second->MarkForRemoval();
			MapUpdate.MarkItemForRemoval(ProcessIt->second->TrafficItem->ItemId);
		}

		if (auto const& Process = ProcessIt->second; Process->DueForRemoval())
		{
			bRemovedAny = true;
			spdlog::debug("Removing process {}.", ProcessIt->first);
			TrafficCounter.PushIncomingTraffic(0); // Force state update
			Process->ParentApp->PushIncomingTraffic(0);

			// If this process exited, but there are still open sockets left,
			// they most likely now belong to a child process. Since those
			// processes might be running as root, and we are not running as root
			// we can't look them up via /proc/. So instead we sent these endpoints
			// to the ip link process which does run as root, which will check
			// what processes (if any) own these ports now. Not exactly a good
			// solution but the best I could think of for now.
			WLookupEndpointsMsg LookupMsg{};

			// When cleaning up a process, we have to
			//  - Remove all its sockets from the Sockets map
			//  - Remove the process from its parent application's Processes map
			//  - Remove the process from the Processes map
			//  - Remove any rules associated with the process and its sockets
			//  - Reparent any leftover sockets in case this process forked
			for (auto const& [SocketCookie, Socket] : Process->TrafficItem->Sockets)
			{
				TrafficItems.erase(Socket->ItemId);
				MapUpdate.AddItemRemoval(Socket->ItemId);
				if (auto SocketCounter = Sockets.find(SocketCookie); SocketCounter != Sockets.end())
				{
					if (Socket->ConnectionState != ESocketConnectionState::Closed)
					{
						spdlog::info("Forked socket {} for {}", Process->ParentApp->TrafficItem->ApplicationName,
							Socket->SocketTuple.ToString());
						// This process exited, but the socket was not closed via the close event sent from ebppf
						// that indicates that a forked child process owns the socket now
						OrphanedSockets[Socket->SocketTuple.LocalEndpoint] = SocketCounter->second;
						LookupMsg.Endpoints.emplace_back(Socket->SocketTuple.LocalEndpoint);
					}
					for (auto const& Tuple : SocketCounter->second->UDPPerConnectionCounters | std::views::values)
					{
						TrafficItems.erase(Tuple->TrafficItem->ItemId);
						MapUpdate.AddItemRemoval(Tuple->TrafficItem->ItemId);
						WNetworkEvents::GetInstance().OnUDPTupleRemoved(Tuple);
					}
					SocketCounter->second->UDPPerConnectionCounters.clear();
					WNetworkEvents::GetInstance().OnSocketRemoved(SocketCounter->second);
				}
				Socket->UDPPerConnectionTraffic.clear();
				Sockets.erase(SocketCookie);
			}
			WIPLink::GetInstance().SendLookupMessage(LookupMsg);
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

	// Re-fetch all currently used sockets from /proc/
	SocketStateParser.ParseData();

	auto IsStaleSocket = [this](std::shared_ptr<WSocketCounter> const& Socket) {
		// So technically we should never have to clean up sockets in this way,
		// so we first make sure the socket has not received any data for 30 seconds
		// at that point the normal cleanup logic should've jumped in,
		// if not we'll do it here but also log it
		if (Socket->IsMarkedForRemoval() || Socket->GetInactiveCounter() < 30)
		{
			return false;
		}

		// If the socket is in an unknown state, we consider it stale and remove it to avoid stale entries in the UI
		if (Socket->TrafficItem->SocketType == ESocketType::Unknown
			|| Socket->TrafficItem->ConnectionState == ESocketConnectionState::Unknown
			|| Socket->TrafficItem->SocketTuple.Protocol == EProtocol::Unknown)
		{
			spdlog::debug("Removing socket because of unknown state");
			return true;
		}

		// If the socket's local port is not in use anymore, we consider it stale (except for ICMP which doesn't have
		// ports)
		if (!SocketStateParser.IsUsedPort(Socket->TrafficItem->SocketTuple.LocalEndpoint.Port)
			&& Socket->TrafficItem->SocketTuple.Protocol != EProtocol::ICMP
			&& Socket->TrafficItem->SocketTuple.Protocol != EProtocol::ICMPv6)
		{
			spdlog::debug("Removing socket because its port is no longer in use");
			return true;
		}

		return false;
	};

	for (auto SocketIt = Sockets.begin(); SocketIt != Sockets.end();)
	{
		if (auto const& Socket = SocketIt->second; Socket->DueForRemoval())
		{
			bRemovedAny = true;

			// When cleaning up a socket, we have to
			//  - Remove it from its parent process's Sockets map
			//  - Remove it from the Sockets map
			WNetworkEvents::GetInstance().OnSocketRemoved(Socket);
			for (auto const& Tuple : Socket->UDPPerConnectionCounters | std::views::values)
			{
				WNetworkEvents::GetInstance().OnUDPTupleRemoved(Tuple);
			}
			Socket->ParentProcess->TrafficItem->Sockets.erase(SocketIt->first);
			TrafficItems.erase(Socket->TrafficItem->ItemId);
			MapUpdate.AddItemRemoval(Socket->TrafficItem->ItemId);
			for (auto const& TupleCounter : Socket->UDPPerConnectionCounters | std::views::values)
			{
				TrafficItems.erase(TupleCounter->TrafficItem->ItemId);
				MapUpdate.AddItemRemoval(TupleCounter->TrafficItem->ItemId);
			}
			Socket->UDPPerConnectionCounters.clear();
			Socket->TrafficItem->UDPPerConnectionTraffic.clear();
			SocketIt = Sockets.erase(SocketIt);
		}
		// Remove sockets in an unknown state
		else if (IsStaleSocket(Socket))
		{
			// todo: ideally we would never end up here
			spdlog::warn("Removing unknown socket with id {}, tuple: {}, app: {}", SocketIt->first,
				Socket->TrafficItem->SocketTuple.ToString(),
				Socket->ParentProcess->ParentApp->TrafficItem->ApplicationName);
			Socket->MarkForRemoval();
		}
		else
		{
			for (auto TupleIt = Socket->UDPPerConnectionCounters.begin();
				TupleIt != Socket->UDPPerConnectionCounters.end();)
			{
				auto const& TupleCounter = TupleIt->second;
				if (TupleCounter->DueForRemoval())
				{
					spdlog::debug("Removed tuple {} -> {}", Socket->TrafficItem->SocketTuple.LocalEndpoint.ToString(),
						TupleIt->first.ToString());
					bRemovedAny = true;
					WNetworkEvents::GetInstance().OnUDPTupleRemoved(TupleCounter);
					TrafficItems.erase(TupleCounter->TrafficItem->ItemId);
					MapUpdate.AddItemRemoval(TupleCounter->TrafficItem->ItemId);
					Socket->TrafficItem->EraseTuple(TupleIt->first);

					TupleIt = Socket->UDPPerConnectionCounters.erase(TupleIt);
				}
				else
				{
					++TupleIt;
				}
			}
			++SocketIt;
		}
	}

	// Remove applications that have no remaining processes
	for (auto AppIt = Applications.begin(); AppIt != Applications.end();)
	{
		if (AppIt->second->TrafficItem->Processes.empty())
		{
			bRemovedAny = true;
			spdlog::debug("Removing application '{}' ({}).", AppIt->second->TrafficItem->ApplicationName, AppIt->first);
			MapUpdate.AddItemRemoval(AppIt->second->TrafficItem->ItemId);
			TrafficItems.erase(AppIt->second->TrafficItem->ItemId);
			SystemItem->Applications.erase(AppIt->first);
			AppIt = Applications.erase(AppIt);
		}
		else
		{
			++AppIt;
		}
	}

	if (bRemovedAny)
	{
		auto NodeCount = Applications.size() + Processes.size() + Sockets.size();
		spdlog::debug("{} nodes remain in the system map after cleanup.", NodeCount);
	}
	if (WTime::GetEpochSeconds() - LastCleanupMessageTime > 5)
	{
		LastCleanupMessageTime = WTime::GetEpochSeconds();
		auto DiffSocketCount = OldSocketCount - Sockets.size();
		auto DiffProcessCount = OldProcessCount - Processes.size();
		auto DiffTrafficItemCount = OldTrafficItemCount - TrafficItems.size();

		spdlog::debug("Cleanup removed {} sockets({} -> {}), {} processes ({} -> {}), and {} traffic items ({} -> {}).",
			DiffSocketCount, OldSocketCount, Sockets.size(), DiffProcessCount, OldProcessCount, Processes.size(),
			DiffTrafficItemCount, OldTrafficItemCount, TrafficItems.size());
	}
}