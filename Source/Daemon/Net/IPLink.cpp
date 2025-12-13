//
// Created by usr on 02/12/2025.
//

#include "IPLink.hpp"

#include <cstdlib>
#include <limits.h>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>
#include <cereal/types/memory.hpp>

#include "NetworkInterface.hpp"
#include "DaemonConfig.hpp"
#include "ErrnoUtil.hpp"
#include "Data/Counters.hpp"
#include "Data/NetworkEvents.hpp"
#include "IPLinkMsg.hpp"

// POSIX includes for non-blocking detached process launch
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

// Helper: launch a detached process (double-fork). The child is forked while this process
// still has root privileges, so the launched process will inherit root (UID=0) and keep it
// independently of this process dropping privileges later.
static bool LaunchDetachedProcess(std::string const& cmd)
{
	pid_t pid = fork();
	if (pid < 0)
	{
		spdlog::error("fork() failed: {}", WErrnoUtil::StrError());
		return false;
	}

	if (pid > 0)
	{
		// parent: wait for the intermediate child to exit so it doesn't become a zombie.
		int status = 0;
		if (waitpid(pid, &status, 0) < 0)
		{
			spdlog::warn("waitpid failed for intermediate child: {}", WErrnoUtil::StrError());
			// Non-fatal — the grandchild will continue running. Still return true because
			// the intention to launch succeeded.
		}
		return true;
	}

	// First child.
	// Create a new session so the child is no longer a process group member of the parent.
	if (setsid() < 0)
	{
		_exit(1);
	}

	// Double-fork: fork again and have the first child exit. The grandchild is fully detached.
	pid_t pid2 = fork();
	if (pid2 < 0)
	{
		_exit(1);
	}
	if (pid2 > 0)
	{
		// first child exits
		_exit(0);
	}

	// Grandchild: fully detached, still has the same UIDs / capabilities as the parent at fork time.
	umask(0);

	// Redirect stdio to /dev/null so child doesn't hold terminal fds.
	int fd = open("/dev/null", O_RDWR);
	if (fd >= 0)
	{
		dup2(fd, STDIN_FILENO);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);
		if (fd > 2)
			close(fd);
	}

	// Execute via /bin/sh -c to keep existing command string parsing.
	execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
	// If exec fails, exit with 127.
	_exit(127);
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

	// for (auto Port : RoutedPorts)
	// {
	// 	SYSFMT2("tc filter delete dev {} parent 1: protocol ip prio {} u32 match ip dport {} 0xffff flowid 1:{}",
	// 		IfName, kIngressPortFilterPriority, Port, MinorId);
	// }
}

bool WIPLink::SetupHTBLimitClass(
	std::shared_ptr<WBandwidthLimit> const& Limit, std::string const& IfName, bool bAttachMarkFilter)
{
	spdlog::info("Setting up HTB class on interface {}: classid=1:{}, mark=0x{:x}, rate={} B/s", IfName, Limit->MinorId,
		Limit->Mark, Limit->RateLimit);
	SYSFMT("tc class replace dev {} parent 1:1 classid 1:{} htb rate {}bit ceil {}bit", IfName, Limit->MinorId,
		static_cast<uint64_t>(Limit->RateLimit * 8), static_cast<uint64_t>(Limit->RateLimit * 8));

	if (bAttachMarkFilter)
	{
		SYSFMT("tc filter replace dev {} parent 1: protocol ip pref 1 handle 0x{:x} fw classid 1:{}", IfName,
			Limit->Mark, Limit->MinorId);
	}
	return true;
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

	for (int i = 0; i < 16; ++i)
	{
		IpProcSecret += static_cast<char>('A' + (rand() % 26));
	}
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
	std::string Cmd = fmt::format("{} {} {} {} {}", IPLinkProcPath, WDaemonConfig::GetInstance().IpLinkProcSocketPath,
		IpProcSecret, IfbDev, IngressInterface);

	spdlog::info("Launching waechter-iplink process: {}", Cmd);
	// Start the process
	if (!LaunchDetachedProcess(Cmd))
	{
		spdlog::error("Failed to start waechter-iplink process");
		return false;
	}

	IpProcSocket = std::make_unique<WClientSocket>(WDaemonConfig::GetInstance().IpLinkProcSocketPath);

	for (int i = 0; i < 100; ++i)
	{
		if (IpProcSocket->Connect())
		{
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
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

bool WIPLink::SetupEgressHTBClass(std::shared_ptr<WBandwidthLimit> const& Limit)
{
	return SetupHTBLimitClass(Limit, WDaemonConfig::GetInstance().NetworkInterfaceName, true);
}

bool WIPLink::SetupIngressHTBClass(std::shared_ptr<WBandwidthLimit> const& Limit)
{
	return SetupHTBLimitClass(Limit, IfbDev, false);
}

bool WIPLink::SetupIngressPortRouting(WTrafficItemId Item, uint32_t QDiscId, uint16_t Dport)
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
			return true;
		}
		WIPLinkMsg Msg{};
		Msg.Type = EIPLinkMsgType::ConfigurePortRouting;
		Msg.Secret = IpProcSecret;
		Msg.SetupPortRouting = std::make_shared<WConfigurePortRouting>();
		Msg.SetupPortRouting->Dport = Dport;
		Msg.SetupPortRouting->QDiscId = QDiscId;
		Msg.SetupPortRouting->bReplace = true;
		IpProcSocket->SendMessage(Msg);

		Limit.Port = Dport;
		Limit.QDiscId = QDiscId;

		return true;
	}
	WIngressPortRouting NewRouting;
	NewRouting.Port = Dport;
	NewRouting.QDiscId = QDiscId;
	IngressPortRoutings[Item] = NewRouting;

	WIPLinkMsg Msg{};
	Msg.Type = EIPLinkMsgType::ConfigurePortRouting;
	Msg.Secret = IpProcSecret;
	Msg.SetupPortRouting = std::make_shared<WConfigurePortRouting>();
	Msg.SetupPortRouting->Dport = Dport;
	Msg.SetupPortRouting->QDiscId = QDiscId;
	IpProcSocket->SendMessage(Msg);
	return true;
}

bool WIPLink::RemoveIngressPortRouting(WTrafficItemId Item)
{
	std::lock_guard Lock(Mutex);
	if (!IngressPortRoutings.contains(Item))
	{
		return false;
	}

	auto&      Limit = IngressPortRoutings[Item];
	WIPLinkMsg Msg{};
	Msg.Type = EIPLinkMsgType::ConfigurePortRouting;
	Msg.Secret = IpProcSecret;
	Msg.SetupPortRouting = std::make_shared<WConfigurePortRouting>();
	Msg.SetupPortRouting->Dport = Limit.Port;
	Msg.SetupPortRouting->QDiscId = Limit.QDiscId;
	Msg.SetupPortRouting->bRemove = true;
	IpProcSocket->SendMessage(Msg);
	IngressPortRoutings.erase(Item);
	return true;
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
