//
// Created by usr on 26/10/2025.
//

#include "SystemMap.hpp"

#include <spdlog/spdlog.h>
#include <fstream>
#include <ranges>
#include <algorithm>
#include <json11.hpp>

#include "ApplicationMap.hpp"
#include "ProcessMap.hpp"
#include "SocketInfo.hpp"

static std::string ReadProc(std::string const& Path)
{
	std::ifstream FileStream(Path, std::ios::in | std::ios::binary);
	if (!FileStream)
		return {};

	// read entire file
	std::string Content((std::istreambuf_iterator<char>(FileStream)), std::istreambuf_iterator<char>());
	if (Content.empty())
	{
		return "";
	}

	// replace NUL separators with spaces
	std::ranges::replace(Content, '\0', ' ');

	// remove a trailing space that comes from the final NUL (if any)
	if (!Content.empty() && Content.back() == ' ')
		Content.pop_back();

	return Content;
}

WSystemMap::WSystemMap()
{
	// Get hostname
	HostName = ReadProc("/proc/sys/kernel/hostname");
	if (HostName.empty())
	{
		HostName = "System";
	}
}

std::shared_ptr<WSocketInfo> WSystemMap::MapSocket(WSocketCookie SocketCookie, WProcessId PID, bool bSilentFail)
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
	auto        CmdLine = ReadProc(CmdLinePath);
	auto        Comm = ReadProc(CommPath);

	// Remove trailing newline from Comm if present
	if (!Comm.empty() && Comm.back() == '\n')
	{
		Comm.pop_back();
	}

	auto App = FindOrMapApplication(Comm);
	auto Process = App->FindOrMapChildProcess(PID, CmdLine);
	auto SocketInfo = Process->FindOrMapSocket(SocketCookie);

	if (SocketInfo)
	{
		Sockets[SocketCookie] = SocketInfo;
	}
	return SocketInfo;
}

std::shared_ptr<WApplicationMap> WSystemMap::FindOrMapApplication(std::string const& AppName)
{
	auto It = Applications.find(AppName);
	if (It != Applications.end())
	{
		return It->second;
	}
	spdlog::debug("Mapped new application: {}", AppName);
	auto AppMap = std::make_shared<WApplicationMap>(AppName, AppName);
	Applications.emplace(AppName, AppMap);
	return AppMap;
}

void WSystemMap::RefreshAllTrafficCounters()
{
	std::lock_guard Lock(DataMutex);
	Cleanup();
	RefreshTrafficCounter();
	for (auto& [AppName, AppMap] : Applications)
	{
		AppMap->RefreshAllTrafficCounters();
	}
}

void WSystemMap::PushIncomingTraffic(WBytes Bytes, WSocketCookie SocketCookie)
{
	std::lock_guard Lock(DataMutex);
	CountIncomingTraffic(Bytes);

	if (auto It = Sockets.find(SocketCookie); It != Sockets.end())
	{
		auto Socket = It->second;
		Socket->CountIncomingTraffic(Bytes);
		if (Socket->ParentProcess)
		{
			Socket->ParentProcess->CountIncomingTraffic(Bytes);
			if (Socket->ParentProcess->GetParentApplicationMap())
			{
				Socket->ParentProcess->GetParentApplicationMap()->CountIncomingTraffic(Bytes);
			}
		}
	}
}

void WSystemMap::PushOutgoingTraffic(WBytes Bytes, WSocketCookie SocketCookie)
{
	std::lock_guard Lock(DataMutex);
	CountOutgoingTraffic(Bytes);

	if (auto It = Sockets.find(SocketCookie); It != Sockets.end())
	{
		auto Socket = It->second;
		Socket->CountOutgoingTraffic(Bytes);
		if (Socket->ParentProcess)
		{
			Socket->ParentProcess->CountOutgoingTraffic(Bytes);
			if (Socket->ParentProcess->GetParentApplicationMap())
			{
				Socket->ParentProcess->GetParentApplicationMap()->CountOutgoingTraffic(Bytes);
			}
		}
	}
}

std::string WSystemMap::ToJson()
{
	std::lock_guard Lock(DataMutex);
	WJson::object   SystemTrafficTree;

	SystemTrafficTree[JSON_KEY_DOWNLOAD] = GetTrafficCounter().GetDownloadSpeed();
	SystemTrafficTree[JSON_KEY_UPLOAD] = GetTrafficCounter().GetUploadSpeed();
	SystemTrafficTree[JSON_KEY_HOSTNAME] = HostName;

	WJson::array ApplicationsArray;

	for (auto& [AppName, AppMap] : Applications)
	{

		if (AppMap->GetChildProcesses().empty())
		{
			continue;
		}
		WJson::object AppJson;
		AppMap->ToJson(AppJson);
		ApplicationsArray.emplace_back(AppJson);
	}
	SystemTrafficTree[JSON_KEY_APPS] = ApplicationsArray;

	return WJson(SystemTrafficTree).dump();
}

std::string WSystemMap::UpdateJson()
{
	std::lock_guard Lock(DataMutex);
	WJson::array    UpdateItems;

	auto MakeUpdateItem = [&](ITrafficItem const& Item, WJson::object& OutJson) {
		OutJson[JSON_KEY_TYPE] = MT_TrafficData;
		OutJson[JSON_KEY_ID] = static_cast<double>(Item.GetItemId());
		OutJson[JSON_KEY_DOWNLOAD] = Item.GetTrafficCounter().GetDownloadSpeed();
		OutJson[JSON_KEY_UPLOAD] = Item.GetTrafficCounter().GetUploadSpeed();
	};

	WJson::object SystemUpdate;
	SystemUpdate[JSON_KEY_TYPE] = MT_TrafficData;
	SystemUpdate[JSON_KEY_DOWNLOAD] = GetTrafficCounter().GetDownloadSpeed();
	SystemUpdate[JSON_KEY_UPLOAD] = GetTrafficCounter().GetUploadSpeed();
	SystemUpdate[JSON_KEY_ID] = 0; // system/root node ID is 0
	UpdateItems.emplace_back(SystemUpdate);

	for (auto& AppMap : Applications | std::views::values)
	{
		if (!AppMap->HasNewData())
		{
			continue;
		}
		WJson::object AppJson;
		MakeUpdateItem(*AppMap, AppJson);
		UpdateItems.emplace_back(AppJson);
		for (auto& ProcessMap : AppMap->GetChildProcesses() | std::views::values)
		{
			if (!ProcessMap->HasNewData())
			{
				continue;
			}
			WJson::object ProcJson;
			MakeUpdateItem(*ProcessMap, ProcJson);
			UpdateItems.emplace_back(ProcJson);
			for (auto& SocketInfo : ProcessMap->GetSockets() | std::views::values)
			{
				if (!SocketInfo->HasNewData())
				{
					continue;
				}
				WJson::object SockJson;
				MakeUpdateItem(*SocketInfo, SockJson);
				UpdateItems.emplace_back(SockJson);
			}
		}
	}
	spdlog::info("System Map Updated with {} items", UpdateItems.size());
	return WJson(UpdateItems).dump();
}

void WSystemMap::ClearDirtyFlags()
{
	std::lock_guard Lock(DataMutex);
	ResetNewDataFlag();
	for (auto& [AppName, AppMap] : Applications)
	{
		AppMap->ResetNewDataFlag();
		for (auto& [PID, ProcessMap] : AppMap->GetChildProcesses())
		{
			ProcessMap->ResetNewDataFlag();
			for (auto& [SocketCookie, SocketInfo] : ProcessMap->GetSockets())
			{
				SocketInfo->ResetNewDataFlag();
			}
		}
	}
}

void WSystemMap::Cleanup()
{
	for (auto& [AppName, AppMap] : Applications)
	{
		auto& ChildProcesses = AppMap->GetChildProcesses();
		for (auto It = ChildProcesses.begin(); It != ChildProcesses.end();)
		{
			auto& ProcessMap = It->second;

			if (ProcessMap->GetTrafficCounter().GetState() == CS_PendingRemoval)
			{
				spdlog::info("Removing process {} from application {}", ProcessMap->GetPID(), AppName);
				// Remove sockets from system map
				for (auto& [SocketCookie, SocketInfo] : ProcessMap->GetSockets())
				{
					Sockets.erase(SocketCookie);
				}
				It = ChildProcesses.erase(It);
			}
			else
			{
				++It;
			}
		}
	}
}