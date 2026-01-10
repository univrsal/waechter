/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string>
#include <cstring>
#include <spdlog/spdlog.h>
#include <cereal/archives/binary.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <thread>
#include <spdlog/fmt/fmt.h>
#include <atomic>
#include <filesystem>
#include <condition_variable>
#include <mutex>
#include <deque>
#include <vector>

#include "Socket.hpp"
#include "ErrnoUtil.hpp"
#include "IPLinkMsg.hpp"
#include "SignalHandler.hpp"

// Sole purpose of this executable is to run `tc` commands which require root
// Commands are sent via unix socket from waechterd after it has dropped privileges

#define SYSFMT(_fmt, ...)                                                                                             \
	if (auto RC = SafeSystem(fmt::format(_fmt, __VA_ARGS__).c_str()); RC != 0)                                        \
	{                                                                                                                 \
		auto CmdFormatted = fmt::format(_fmt, __VA_ARGS__);                                                           \
		spdlog::error("Failed to execute system command '{}': {} (RC={})", CmdFormatted, WErrnoUtil::StrError(), RC); \
		return false;                                                                                                 \
	}

#define SYSFMT2(_fmt, ...)                                                                                            \
	if (auto RC = SafeSystem(fmt::format(_fmt, __VA_ARGS__).c_str()); RC != 0)                                        \
	{                                                                                                                 \
		auto CmdFormatted = fmt::format(_fmt, __VA_ARGS__);                                                           \
		spdlog::error("Failed to execute system command '{}': {} (RC={})", CmdFormatted, WErrnoUtil::StrError(), RC); \
	}

struct WMsgQueue;

static bool RemoveHtbClass(std::shared_ptr<WRemoveHtbClassMsg> const& RemoveHtbClass);

static void ProcessMessage(WMsgQueue& Queue, WIPLinkMsg const& Msg, WSignalHandler& Handler, std::string const& IfbDev);

struct HTBMapEntry
{
	uint16_t                            UseCount{};
	std::shared_ptr<WRemoveHtbClassMsg> PendingRemoveMsg{};

	~HTBMapEntry()
	{
		if (PendingRemoveMsg != nullptr)
		{
			RemoveHtbClass(PendingRemoveMsg);
		}
	}
};

struct WHTBMap
{
	std::mutex                                                 Mutex;
	std::unordered_map<uint16_t, std::shared_ptr<HTBMapEntry>> HTBUseCounter; // minor id -> amount of ports using it

	void AddUsage(uint16_t MinorId)
	{
		std::lock_guard Lock(Mutex);
		if (HTBUseCounter.find(MinorId) == HTBUseCounter.end())
		{
			HTBUseCounter[MinorId] = std::make_shared<HTBMapEntry>();
		}
		HTBUseCounter[MinorId]->UseCount++;
	}

	bool MarkPendingRemove(uint16_t MinorId, std::shared_ptr<WRemoveHtbClassMsg> const& Msg)
	{
		std::lock_guard Lock(Mutex);
		auto            It = HTBUseCounter.find(MinorId);
		if (It != HTBUseCounter.end())
		{
			It->second->PendingRemoveMsg = Msg;
			return true;
		}
		// just remove immediately
		return RemoveHtbClass(Msg);
	}

	void RemoveUsage(uint16_t MinorId)
	{
		std::lock_guard Lock(Mutex);
		auto            It = HTBUseCounter.find(MinorId);
		if (It != HTBUseCounter.end())
		{
			if (It->second->UseCount > 0)
			{
				It->second->UseCount--;
			}
			if (It->second->UseCount == 0)
			{
				HTBUseCounter.erase(It);
			}
		}
	}
};

// A tiny bounded-ish queue to avoid unbounded RAM usage if the sender outruns `tc`.
// If it grows too large, we log warnings so it's visible in the logs.
struct WMsgQueue
{
	std::mutex              Mutex;
	std::condition_variable Cv;
	std::deque<WIPLinkMsg>  Queue;
	std::atomic<bool>       bStop{ false };

	WHTBMap HtbUsageMap;

	// soft limit; we only warn when exceeded
	size_t WarnLimit{ 256 };

	void Push(WIPLinkMsg&& Msg)
	{
		{
			std::lock_guard Lock(Mutex);
			Queue.emplace_back(std::move(Msg));
			if (Queue.size() == WarnLimit)
			{
				spdlog::warn("[tc] Message queue reached {} items; tc execution might be slow", Queue.size());
			}
		}
		Cv.notify_one();
	}

	// Returns false if stopping and queue is empty.
	bool Pop(WIPLinkMsg& Out)
	{
		std::unique_lock Lock(Mutex);
		Cv.wait(Lock, [&] { return bStop.load() || !Queue.empty(); });
		if (Queue.empty())
		{
			return false;
		}
		Out = std::move(Queue.front());
		Queue.pop_front();
		return true;
	}

	void Stop()
	{
		bStop = true;
		Cv.notify_all();
	}
};

static int SafeSystem(std::string const& Command)
{
	// Check if the command contains any dangerous characters
	for (char c : Command)
	{
		if (!std::isalnum(c) && c != ' ' && c != '_' && c != '-' && c != ':' && c != '=')
		{
			spdlog::error("Unsafe character '{}' detected in command: {}", c, Command);
			return -1;
		}
	}
	return system(Command.c_str());
}

static std::string SanitizeInterfaceName(std::string const& IfName)
{
	// only allow alphanumeric characters, underscores and dashes to prevent command injection
	std::string Result = IfName;
	Result.erase(
		std::remove_if(Result.begin(), Result.end(), [](char c) { return !std::isalnum(c) && c != '_' && c != '-'; }),
		Result.end());
	return Result;
}

static bool SetupHtbClass(std::shared_ptr<WSetupHtbClassMsg> const& SetupHtbClass)
{
	auto const IfName = SanitizeInterfaceName(SetupHtbClass->InterfaceName);
	spdlog::info("Setting up HTB class on interface {}: classid=1:{}, mark=0x{:x}, rate={} B/s", IfName,
		SetupHtbClass->MinorId, SetupHtbClass->Mark, SetupHtbClass->RateLimit);
	SYSFMT("tc class replace dev {} parent 1:1 classid 1:{} htb rate {}bit ceil {}bit", IfName, SetupHtbClass->MinorId,
		static_cast<uint64_t>(SetupHtbClass->RateLimit * 8), static_cast<uint64_t>(SetupHtbClass->RateLimit * 8));

	if (SetupHtbClass->bAddMarkFilter)
	{
		SYSFMT("tc filter replace dev {} parent 1: protocol ip pref 1 handle 0x{:x} fw classid 1:{}", IfName,
			SetupHtbClass->Mark, SetupHtbClass->MinorId);
	}
	return true;
}

static bool RemoveHtbClass(std::shared_ptr<WRemoveHtbClassMsg> const& RemoveHtbClass)
{
	auto const& IfName = SanitizeInterfaceName(RemoveHtbClass->InterfaceName);
	spdlog::debug("Removing HTB class on interface {}: classid=1:{}, mark=0x{:x}", IfName, RemoveHtbClass->MinorId,
		RemoveHtbClass->Mark);
	if (RemoveHtbClass->bIsUpload)
	{
		SYSFMT2("tc filter delete dev {} parent 1: protocol ip pref 1 handle 0x{:x} fw classid 1:{}", IfName,
			RemoveHtbClass->Mark, RemoveHtbClass->MinorId);
	}
	SYSFMT("tc class delete dev {} classid 1:{}", IfName, RemoveHtbClass->MinorId);
	return true;
}

static bool ConfigurePortRouting(
	std::shared_ptr<WConfigurePortRouting> const& SetupPortRouting, std::string const& IfbDev)
{
	int const Priority = 1;
	if (SetupPortRouting->bRemove)
	{
		SYSFMT("tc filter del dev {} protocol ip parent 1: prio {} handle {} flower", IfbDev, Priority,
			SetupPortRouting->Handle);
		SYSFMT("tc filter del dev {} protocol ip parent 1: prio {} handle {} flower", IfbDev, Priority,
			SetupPortRouting->Handle + 1);
		spdlog::debug("Removed ingress port routing on interface {}: dport={}, qdisc id={}", IfbDev,
			SetupPortRouting->Dport, SetupPortRouting->QDiscId);
	}
	else
	{
		spdlog::info("Setting up ingress port routing on interface {}: dport={}, qdisc id={}", IfbDev,
			SetupPortRouting->Dport, SetupPortRouting->QDiscId);
		SYSFMT(
			"tc filter add dev {} parent 1: protocol ip prio {} handle {} flower ip_proto tcp dst_port {} classid 1:{}",
			IfbDev, Priority, SetupPortRouting->Handle, SetupPortRouting->Dport, SetupPortRouting->QDiscId);
		SYSFMT(
			"tc filter add dev {} parent 1: protocol ip prio {} handle {} flower ip_proto udp dst_port {} classid 1:{}",
			IfbDev, Priority, SetupPortRouting->Handle + 1, SetupPortRouting->Dport, SetupPortRouting->QDiscId);
	}
	return true;
}

static bool Init(std::string const& IfbDev, std::string const& IngressInterface)
{
	// Create the ifb0 interface
	SafeSystem(fmt::format("ip link delete {}", IfbDev));
	SafeSystem(fmt::format("ip link add {0} type ifb", IfbDev));
	SYSFMT("ip link set {0} up", IfbDev);

	// Ingress HTB setup
	SYSFMT("tc qdisc replace dev {} root handle 1: htb default 0x10", IfbDev);
	SYSFMT("tc class replace dev {} parent 1: classid 1:1 htb rate 1Gbit ceil 1Gbit", IfbDev);

	// leaf class for ifb
	SYSFMT("tc class replace dev {} parent 1:1 classid 1:10 "
		   "htb rate 1Gbit ceil 1Gbit",
		IfbDev);
	// Default filter
	SYSFMT("tc filter replace dev {} parent 1: prio 100 protocol all u32 match u32 0 0 flowid 1:10", IfbDev);

	// Redirect ingress traffic to ifb0
	SYSFMT("tc qdisc replace dev {} handle ffff: ingress", IngressInterface);
	SYSFMT(
		"tc filter replace dev {} parent ffff: protocol all prio 100 u32 match u32 0 0 action mirred egress redirect dev {}",
		IngressInterface, IfbDev);

	return true;
}

static void WorkerThreadMain(WMsgQueue& Queue, WSignalHandler& Handler, std::string const& IfbDev)
{
	WIPLinkMsg Msg;
	while (!Handler.bStop)
	{
		// Wait for work (or stop)
		if (!Queue.Pop(Msg))
		{
			break;
		}
		ProcessMessage(Queue, Msg, Handler, IfbDev);
	}
}

static void ProcessMessage(WMsgQueue& Queue, WIPLinkMsg const& Msg, WSignalHandler& Handler, std::string const& IfbDev)
{
	if (Msg.Type == EIPLinkMsgType::Exit)
	{
		spdlog::info("Received exit message, shutting down");
		Handler.bStop = true;
		return;
	}
	if (Msg.Type == EIPLinkMsgType::SetupHtbClass && Msg.SetupHtbClass)
	{
		if (!SetupHtbClass(Msg.SetupHtbClass))
		{
			spdlog::error("Failed to setup HTB class");
		}
		return;
	}
	if (Msg.Type == EIPLinkMsgType::RemoveHtbClass && Msg.RemoveHtbClass)
	{
		if (!Queue.HtbUsageMap.MarkPendingRemove(Msg.RemoveHtbClass->MinorId, Msg.RemoveHtbClass))
		{
			spdlog::error("Failed to remove HTB class");
		}
		return;
	}
	if (Msg.Type == EIPLinkMsgType::ConfigurePortRouting && Msg.SetupPortRouting)
	{
		if (!ConfigurePortRouting(Msg.SetupPortRouting, IfbDev))
		{
			spdlog::error("Failed to configure port routing");
		}
		if (Msg.SetupPortRouting->bRemove)
		{
			Queue.HtbUsageMap.RemoveUsage(Msg.SetupPortRouting->QDiscId);
		}
		else
		{
			Queue.HtbUsageMap.AddUsage(Msg.SetupPortRouting->QDiscId);
		}
		return;
	}
}

void OnDataReceived(WBuffer& RecvBuffer, WSignalHandler& Handler, WMsgQueue& Queue)
{
	// Important: do not execute any slow system calls here. Just deserialize and enqueue.
	// Keep allocations modest.
	std::stringstream SS(std::string(RecvBuffer.GetData(), RecvBuffer.GetReadableSize()));
	WIPLinkMsg        Msg;
	try
	{
		cereal::BinaryInputArchive Iar(SS);
		Iar(Msg);
	}
	catch (std::exception const& e)
	{
		spdlog::error("Failed to deserialize message: {}", e.what());
		return;
	}

	// Fast path: Exit can stop everything quickly.
	if (Msg.Type == EIPLinkMsgType::Exit)
	{
		Handler.bStop = true;
		Queue.Stop();
		return;
	}

	Queue.Push(std::move(Msg));
}

// For bandwidth limiting we need
//  - A new interface ifb0 for ingress redirection
//	- HTB on the main interface (egress)
//	- HTB on the ifb0 interface (ingress)
//	- An ingress qdisc on the main interface that redirects all traffic to ifb
//  - A root qdisc on ifb0 to shape the ingress traffic
//  - A root qdisc on the main interface to shape the egress traffic
//  - A filter on the ingress qdisc to redirect all traffic to ifb0
int main(int Argc, char** Argv)
{
	spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [tc] %v");

	if (Argc < 4)
	{
		spdlog::error("Usage: waechter-iplink [socket path] [ifb dev] [ingress interface]");
		return -1;
	}

	std::string SocketPath = Argv[1];
	std::string IfbDev = SanitizeInterfaceName(Argv[2]);
	std::string IngressInterface = SanitizeInterfaceName(Argv[3]);
	if (std::filesystem::exists(SocketPath))
	{
		unlink(SocketPath.c_str());
	}

	WServerSocket Socket(SocketPath);
	if (!Socket.BindAndListen())
	{
		spdlog::error("Failed to bind and listen on socket {}", SocketPath);
		return -1;
	}

	if (!Init(IfbDev, IngressInterface))
	{
		spdlog::error("Failed to setup");
		return -1;
	}

	spdlog::info("Listening on socket {}. Waiting for daemon to connect", SocketPath);
	auto ClientSocket = Socket.Accept();
	spdlog::info("Daemon connected to IP link process socket");

	WSignalHandler Handler;
	WBuffer        RecvBuffer;
	WMsgQueue      Queue;

	// Create a thread pool to process messages in parallel
	// Using hardware concurrency, but capping at reasonable limit
	size_t ThreadCount = std::min(std::thread::hardware_concurrency(), 8u);
	if (ThreadCount == 0)
	{
		ThreadCount = 4; // fallback default
	}
	spdlog::info("Starting thread pool with {} worker threads", ThreadCount);

	std::vector<std::thread> WorkerPool;
	WorkerPool.reserve(ThreadCount);
	for (size_t i = 0; i < ThreadCount; ++i)
	{
		WorkerPool.emplace_back([&Queue, &Handler, IfbDev] { WorkerThreadMain(Queue, Handler, IfbDev); });
	}

	ClientSocket->OnClosed.connect([&Handler, &Queue]() {
		spdlog::info("Daemon socket closed, exiting");
		Handler.bStop = true;
		Queue.Stop();
		// Don't join here: OnClosed is invoked from the listener thread.
	});
	ClientSocket->OnData.connect([&Handler, &Queue](WBuffer& Data) { OnDataReceived(Data, Handler, Queue); });

	ClientSocket->StartListenThread();

	while (!Handler.bStop)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	Queue.Stop();
	spdlog::info("Waiting for worker threads to finish...");
	for (auto& Worker : WorkerPool)
	{
		if (Worker.joinable())
		{
			Worker.join();
		}
	}
	spdlog::info("All worker threads finished");

	// Cleanup
	SYSFMT2("tc filter delete dev {} parent ffff:", IngressInterface);
	SYSFMT2("tc qdisc delete dev {} root", IfbDev);
	SYSFMT2("ip link delete {}", IfbDev);
	return 0;
}
