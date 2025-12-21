//
// Created by usr on 02/12/2025.
//

#include "IPLink.hpp"

#include <cstdlib>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>
#include <cereal/types/memory.hpp>

#include "NetworkInterface.hpp"
#include "DaemonConfig.hpp"
#include "ErrnoUtil.hpp"
#include "Data/Counters.hpp"
#include "Data/NetworkEvents.hpp"
#include "IPLinkMsg.hpp"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

// Helper: launch a detached process (double-fork). The child is forked while this process
// still has root privileges, so the launched process will inherit root (UID=0) and keep it
// independently of this process dropping privileges later.
static bool LaunchDetachedProcess(std::string const& cmd)
{
	return system(fmt::format("{} & disown", cmd).c_str()) == 0;
}

#define SYSFMT(_fmt, ...)                                                                                             \
	if (auto RC = system(fmt::format(_fmt, __VA_ARGS__).c_str()); RC != 0)                                            \
	{                                                                                                                 \
		auto CmdFormatted = fmt::format(_fmt, __VA_ARGS__);                                                           \
		spdlog::error("Failed to execute system command '{}': {} (RC={})", CmdFormatted, WErrnoUtil::StrError(), RC); \
		return false;                                                                                                 \
	}

#define SYSFMT2(_fmt, ...)                                                                                            \
	if (auto RC = system(fmt::format(_fmt, __VA_ARGS__).c_str()); RC != 0)                                            \
	{                                                                                                                 \
		auto CmdFormatted = fmt::format(_fmt, __VA_ARGS__);                                                           \
		spdlog::error("Failed to execute system command '{}': {} (RC={})", CmdFormatted, WErrnoUtil::StrError(), RC); \
	}

constexpr int kIngressPortFilterPriority = 1;

WBandwidthLimit::WBandwidthLimit(
	uint32_t Mark_, uint16_t MinorId_, WBytesPerSecond RateLimit_, ELimitDirection Direction_)
	: Direction(Direction_), Mark(Mark_), MinorId(MinorId_), RateLimit(RateLimit_)
{
}

WBandwidthLimit::~WBandwidthLimit()
{
	// clean up tc classes and filters
	std::string IfName =
		(Direction == ELimitDirection::Upload) ? WDaemonConfig::GetInstance().NetworkInterfaceName : WIPLink::IfbDev;

	if (Direction == ELimitDirection::Upload)
	{
		SYSFMT2("tc filter delete dev {} parent 1: protocol ip pref 1 handle 0x{:x} fw classid 1:{}", IfName, Mark,
			MinorId);
	}
	SYSFMT2("tc class delete dev {} classid 1:{}", IfName, MinorId);
}

void WIPLink::SetupHTBLimitClass(
	std::shared_ptr<WBandwidthLimit> const& Limit, std::string const& IfName, bool bAttachMarkFilter) const
{
	spdlog::info("Setting up HTB class on interface {}: classid=1:{}, mark=0x{:x}, rate={} B/s", IfName, Limit->MinorId,
		Limit->Mark, Limit->RateLimit);

	WIPLinkMsg Msg{};
	Msg.Type = EIPLinkMsgType::SetupHtbClass;
	Msg.SetupHtbClass = std::make_shared<WSetupHtbClassMsg>();
	Msg.SetupHtbClass->InterfaceName = IfName;
	Msg.SetupHtbClass->Mark = Limit->Mark;
	Msg.SetupHtbClass->MinorId = Limit->MinorId;
	Msg.SetupHtbClass->RateLimit = Limit->RateLimit;
	Msg.SetupHtbClass->bAddMarkFilter = bAttachMarkFilter;
	IpProcSocket->SendMessage(Msg);
}

void WIPLink::OnSocketRemoved(std::shared_ptr<WSocketCounter> const& Socket)
{
	RemoveIngressPortRouting(Socket->TrafficItem->ItemId);
	RemoveUploadLimit(Socket->TrafficItem->ItemId);
}

bool WIPLink::Init()
{
	WNetworkEvents::GetInstance().OnSocketRemoved.connect(
		std::bind(&WIPLink::OnSocketRemoved, this, std::placeholders::_1));

	// Get the path to waechter-iplink, assuming it's in the Net/IPLinkProc subdirectory relative to this executable's
	// directory
	std::string IPLinkProcPath = "waechter-iplink"; // fallback
	char        exe_path[PATH_MAX];
	ssize_t     len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
	if (len > 0)
	{
		exe_path[len] = '\0';
		std::string full_exe_path = exe_path;
		size_t      last_slash = full_exe_path.find_last_of('/');
		if (last_slash != std::string::npos)
		{
			std::string exe_dir = full_exe_path.substr(0, last_slash + 1);
			IPLinkProcPath = exe_dir + "Net/IPLinkProc/waechter-iplink";
		}
	}
	// Launch IPLinkProc as root with arguments [socket path] [secret] [ifb dev] [ingress interface]
	std::string IngressInterface = WDaemonConfig::GetInstance().NetworkInterfaceName;
	std::string Cmd = fmt::format(
		"{} {} {} {}", IPLinkProcPath, WDaemonConfig::GetInstance().IpLinkProcSocketPath, IfbDev, IngressInterface);

	spdlog::info("Launching waechter-iplink process: {}", Cmd);
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
	if (IpProcSocket->GetState() != ES_Connected)
	{
		spdlog::error("Failed to connect to waechter-iplink process socket: {}", WErrnoUtil::StrError());
		return false;
	}

	spdlog::info("IP link process socket connected");

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

void WIPLink::SetupEgressHTBClass(std::shared_ptr<WBandwidthLimit> const& Limit)
{
	SetupHTBLimitClass(Limit, WDaemonConfig::GetInstance().NetworkInterfaceName, true);
}

void WIPLink::SetupIngressHTBClass(std::shared_ptr<WBandwidthLimit> const& Limit)
{
	SetupHTBLimitClass(Limit, IfbDev, false);
}

void WIPLink::SetupIngressPortRouting(WTrafficItemId Item, uint32_t QDiscId, uint16_t Dport)
{
	spdlog::info("Setting up ingress port routing for traffic item ID {}: dport={}, qdisc id={}", Item, Dport, QDiscId);
	std::lock_guard Lock(Mutex);
	if (IngressPortRoutings.contains(Item))
	{
		auto& Limit = IngressPortRoutings[Item];

		if (Limit.Port == Dport && Limit.QDiscId == QDiscId)
		{
			// Already routed
			spdlog::info("Port already routed for traffic item ID {}: dport={}, qdisc id={}", Item, Dport, QDiscId);
			return;
		}
		WIPLinkMsg Msg{};
		Msg.Type = EIPLinkMsgType::ConfigurePortRouting;
		Msg.SetupPortRouting = std::make_shared<WConfigurePortRouting>();
		Msg.SetupPortRouting->Dport = Dport;
		Msg.SetupPortRouting->QDiscId = QDiscId;
		Msg.SetupPortRouting->bReplace = true;
		IpProcSocket->SendMessage(Msg);

		Limit.Port = Dport;
		Limit.QDiscId = QDiscId;

		return;
	}
	spdlog::info("Sending port routing setup to ip link process for traffic item ID {}: dport={}, qdisc id={}", Item,
		Dport, QDiscId);
	WIngressPortRouting NewRouting;
	NewRouting.Port = Dport;
	NewRouting.QDiscId = QDiscId;
	IngressPortRoutings[Item] = NewRouting;

	WIPLinkMsg Msg{};
	Msg.Type = EIPLinkMsgType::ConfigurePortRouting;
	Msg.SetupPortRouting = std::make_shared<WConfigurePortRouting>();
	Msg.SetupPortRouting->Dport = Dport;
	Msg.SetupPortRouting->QDiscId = QDiscId;
	IpProcSocket->SendMessage(Msg);
}

void WIPLink::RemoveIngressPortRouting(WTrafficItemId Item)
{
	std::lock_guard Lock(Mutex);
	if (!IngressPortRoutings.contains(Item))
	{
		return;
	}

	auto&      Limit = IngressPortRoutings[Item];
	WIPLinkMsg Msg{};
	Msg.Type = EIPLinkMsgType::ConfigurePortRouting;
	Msg.SetupPortRouting = std::make_shared<WConfigurePortRouting>();
	Msg.SetupPortRouting->Dport = Limit.Port;
	Msg.SetupPortRouting->QDiscId = Limit.QDiscId;
	Msg.SetupPortRouting->bRemove = true;
	IpProcSocket->SendMessage(Msg);
	IngressPortRoutings.erase(Item);
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

std::shared_ptr<WBandwidthLimit> WIPLink::GetUploadLimit(WTrafficItemId const& ItemId, WBytesPerSecond const& Limit)
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
		return ExistingLimit;
	}
	auto NewLimit = std::make_shared<WBandwidthLimit>(
		NextMark.fetch_add(1), NextMinorId.fetch_add(1), Limit, ELimitDirection::Upload);

	ActiveUploadLimits[ItemId] = NewLimit;

	SetupEgressHTBClass(NewLimit);

	return NewLimit;
}

std::shared_ptr<WBandwidthLimit> WIPLink::GetDownloadLimit(WTrafficItemId const& ItemId, WBytesPerSecond const& Limit)
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
		return ActiveDownloadLimits[ItemId];
	}

	auto NewLimit = std::make_shared<WBandwidthLimit>(
		NextMark.fetch_add(1), NextMinorId.fetch_add(1), Limit, ELimitDirection::Download);
	ActiveDownloadLimits[ItemId] = NewLimit;

	SetupIngressHTBClass(NewLimit);

	return NewLimit;
}
