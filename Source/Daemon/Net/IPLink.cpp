/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "IPLink.hpp"

#include <cstdlib>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>
#include <cereal/types/memory.hpp>
#include <unistd.h>
#include <sys/types.h>

#include "Daemon.hpp"
#include "NetworkInterface.hpp"
#include "DaemonConfig.hpp"
#include "ErrnoUtil.hpp"
#include "Filesystem.hpp"
#include "Data/Counters.hpp"
#include "Data/NetworkEvents.hpp"
#include "IPLinkMsg.hpp"
#include "Data/SystemMap.hpp"
#include "EBPF/EbpfData.hpp"

// Helper: launch a detached process (double-fork). The child is forked while this process
// still has root privileges, so the launched process will inherit root (UID=0) and keep it
// independently of this process dropping privileges later.
static bool LaunchDetachedProcess(std::string const& cmd)
{
	return system(fmt::format("{} &", cmd).c_str()) == 0;
}

constexpr int kIngressPortFilterPriority = 1;

WBandwidthLimit::WBandwidthLimit(
	uint32_t Mark_, uint16_t MinorId_, WBytesPerSecond RateLimit_, ELimitDirection Direction_)
	: Direction(Direction_), Mark(Mark_), MinorId(MinorId_), RateLimit(RateLimit_)
{
}

WBandwidthLimit::~WBandwidthLimit()
{
	spdlog::debug("HTB limit class being removed: classid=1:{}, rate={} B/s", MinorId, RateLimit);
	// clean up tc classes and filters
	std::string IfName =
		(Direction == ELimitDirection::Upload) ? WDaemonConfig::GetInstance().NetworkInterfaceName : WIPLink::IfbDev;

	WIPLinkMsg Msg{};
	Msg.Type = EIPLinkMsgType::RemoveHtbClass;
	Msg.RemoveHtbClass = std::make_shared<WRemoveHtbClassMsg>();
	Msg.RemoveHtbClass->InterfaceName = IfName;
	Msg.RemoveHtbClass->Mark = Mark;
	Msg.RemoveHtbClass->MinorId = MinorId;
	WIPLink::GetInstance().IpProcSocket->SendMessage(Msg);
}

void WIPLink::SetupHTBLimitClass(std::shared_ptr<WBandwidthLimit> const& Limit, std::string const& IfName) const
{
	spdlog::debug("Setting up HTB class on interface {}: classid=1:{}, mark=0x{:x}, rate={} B/s", IfName,
		Limit->MinorId, Limit->Mark, Limit->RateLimit);

	WIPLinkMsg Msg{};
	Msg.Type = EIPLinkMsgType::SetupHtbClass;
	Msg.SetupHtbClass = std::make_shared<WSetupHtbClassMsg>();
	Msg.SetupHtbClass->InterfaceName = IfName;
	Msg.SetupHtbClass->Mark = Limit->Mark;
	Msg.SetupHtbClass->MinorId = Limit->MinorId;
	Msg.SetupHtbClass->RateLimit = Limit->RateLimit;
	IpProcSocket->SendMessage(Msg);
}

void WIPLink::OnSocketRemoved(std::shared_ptr<WSocketCounter> const& Socket)
{
	RemoveIngressPortRouting(Socket->TrafficItem->SocketTuple.LocalEndpoint.Port);
	RemoveDownloadLimit(Socket->TrafficItem->ItemId);
	RemoveUploadLimit(Socket->TrafficItem->ItemId);
}

std::string const WIPLink::IfbDev = "ifb0";

bool WIPLink::Init()
{
	WNetworkEvents::GetInstance().OnSocketRemoved.connect(
		std::bind(&WIPLink::OnSocketRemoved, this, std::placeholders::_1));

	// Get the path to waechter-iplink, assuming it's in the Net/IPLinkProc subdirectory relative to this executable's
	// directory
	std::string IPLinkProcPath = "waechter-iplink"; // fallback
#if WDEBUG
	char    exe_path[PATH_MAX];
	ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
	if (len > 0)
	{
		exe_path[len] = '\0';
		std::string full_exe_path = exe_path;
		size_t      last_slash = full_exe_path.find_last_of('/');
		if (last_slash != std::string::npos)
		{
			std::string exe_dir = full_exe_path.substr(0, last_slash + 1);
			auto        IpLinkProcPath = exe_dir + "Net/IPLinkProc/waechter-iplink";

			if (WFilesystem::Exists(IpLinkProcPath))
			{
				spdlog::debug("Found waechter-iplink at {}", IpLinkProcPath);
				IPLinkProcPath = IpLinkProcPath;
			}
			else
			{
				spdlog::warn(
					"waechter-iplink not found at {}, falling back to just 'waechter-iplink' (relying on PATH)",
					IpLinkProcPath);
			}
		}
	}
#endif
	// Launch IPLinkProc as root with arguments [socket path] [secret] [ifb dev] [ingress interface]
	std::string IngressInterface = WDaemonConfig::GetInstance().IngressNetworkInterfaceName;
	std::string MainInterface = WDaemonConfig::GetInstance().NetworkInterfaceName;
	std::string Cmd = fmt::format("{} {} {} {} {}", IPLinkProcPath, WDaemonConfig::GetInstance().IpLinkProcSocketPath,
		IfbDev, IngressInterface, MainInterface);

	spdlog::debug("Launching waechter-iplink process: {}", Cmd);
	// Start the process
	if (!LaunchDetachedProcess(Cmd))
	{
		spdlog::error("Failed to start waechter-iplink process");
		return false;
	}

	IpProcSocket = std::make_unique<WClientSocket>(WDaemonConfig::GetInstance().IpLinkProcSocketPath);

	for (int i = 0; i < 10; ++i)
	{
		if (IpProcSocket->Connect())
		{
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}
	if (!IpProcSocket->IsConnected())
	{
		spdlog::error("Failed to connect to waechter-iplink process socket: {}", WErrnoUtil::StrError());
		return false;
	}

	spdlog::debug("IP link process socket connected");

	WaechterIngressIfIndex = WNetworkInterface::GetIfIndex(IfbDev);
	return true;
}

bool WIPLink::Deinit()
{
	std::lock_guard Lock(Mutex);
	ActiveUploadLimits.clear();
	ActiveDownloadLimits.clear();
	return true;
}

void WIPLink::SetupEgressHTBClass(std::shared_ptr<WBandwidthLimit> const& Limit) const
{
	SetupHTBLimitClass(Limit, WDaemonConfig::GetInstance().NetworkInterfaceName);
}

void WIPLink::SetupIngressHTBClass(std::shared_ptr<WBandwidthLimit> const& Limit) const
{
	SetupHTBLimitClass(Limit, IfbDev);
}

void WIPLink::SetupIngressPortRouting(WTrafficItemId, uint32_t DownloadMark, uint16_t Dport)
{
	auto const& Data = WDaemon::GetInstance().GetEbpfObj().GetData();

	if (!Data->SocketMarks->Update(Dport, static_cast<uint16_t>(DownloadMark)))
	{
		spdlog::error("Failed to add socket mark for port {}", Dport);
	}
}

void WIPLink::RemoveIngressPortRouting(uint16_t Dport)
{
	auto const& Data = WDaemon::GetInstance().GetEbpfObj().GetData();

	if (!Data->SocketMarks->Update(Dport, 0))
	{
		spdlog::error("Failed to remove socket mark for port {}", Dport);
	}
}

void WIPLink::RemoveUploadLimit(WTrafficItemId const& ItemId)
{
	std::lock_guard Lock(Mutex);
	if (ActiveUploadLimits.contains(ItemId))
	{
		ActiveUploadLimits.erase(ItemId);
	}
}

void WIPLink::RemoveDownloadLimit(WTrafficItemId const& ItemId)
{
	std::lock_guard Lock(Mutex);
	if (ActiveDownloadLimits.contains(ItemId))
	{
		ActiveDownloadLimits.erase(ItemId);
	}
}

std::shared_ptr<WBandwidthLimit> WIPLink::GetUploadLimit(
	WTrafficItemId const& ItemId, WBytesPerSecond const& Limit, bool* bExists)
{
	std::lock_guard Lock(Mutex);
	if (ActiveUploadLimits.contains(ItemId))
	{
		auto ExistingLimit = ActiveUploadLimits[ItemId];
		if (ExistingLimit->RateLimit != Limit)
		{
			// Update existing limit
			ExistingLimit->RateLimit = Limit;
			SetupEgressHTBClass(ExistingLimit);
		}
		if (bExists)
		{
			*bExists = true;
		}
		return ExistingLimit;
	}
	if (bExists)
	{
		*bExists = false;
	}
	auto NewLimit = std::make_shared<WBandwidthLimit>(
		NextMark.fetch_add(1), NextMinorId.fetch_add(1), Limit, ELimitDirection::Upload);

	ActiveUploadLimits[ItemId] = NewLimit;

	SetupEgressHTBClass(NewLimit);

	return NewLimit;
}

std::shared_ptr<WBandwidthLimit> WIPLink::GetDownloadLimit(
	WTrafficItemId const& ItemId, WBytesPerSecond const& Limit, bool* bExists)
{
	std::lock_guard Lock(Mutex);
	if (ActiveDownloadLimits.contains(ItemId))
	{
		auto ExistingLimit = ActiveDownloadLimits[ItemId];
		if (ExistingLimit->RateLimit != Limit)
		{
			// Update existing limit
			ExistingLimit->RateLimit = Limit;
			SetupIngressHTBClass(ExistingLimit);
		}
		if (bExists)
		{
			*bExists = true;
		}
		return ActiveDownloadLimits[ItemId];
	}
	if (bExists)
	{
		*bExists = false;
	}

	auto NewLimit = std::make_shared<WBandwidthLimit>(
		NextMark.fetch_add(1), NextMinorId.fetch_add(1), Limit, ELimitDirection::Download);
	ActiveDownloadLimits[ItemId] = NewLimit;

	SetupIngressHTBClass(NewLimit);

	return NewLimit;
}
