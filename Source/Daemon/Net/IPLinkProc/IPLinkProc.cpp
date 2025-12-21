//
// Created by usr on 10/12/2025.
//

#include <string>
#include <cstring>
#include <spdlog/spdlog.h>
#include <cereal/archives/binary.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <thread>
#include <spdlog/fmt/fmt.h>
#include <atomic>

#include "Socket.hpp"
#include "ErrnoUtil.hpp"
#include "IPLinkMsg.hpp"
#include "SignalHandler.hpp"

#include <filesystem>

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

// Sole purpose of this executable is to run `tc` commands which require root
// Commands are sent via unix socket from waechterd after it has dropped privileges

bool SetupHtbClass(std::shared_ptr<WSetupHtbClassMsg> const& SetupHtbClass)
{

	spdlog::info("Setting up HTB class on interface {}: classid=1:{}, mark=0x{:x}, rate={} B/s",
		SetupHtbClass->InterfaceName, SetupHtbClass->MinorId, SetupHtbClass->Mark, SetupHtbClass->RateLimit);
	SYSFMT("tc class replace dev {} parent 1:1 classid 1:{} htb rate {}bit ceil {}bit", SetupHtbClass->InterfaceName,
		SetupHtbClass->MinorId, static_cast<uint64_t>(SetupHtbClass->RateLimit * 8),
		static_cast<uint64_t>(SetupHtbClass->RateLimit * 8));

	if (SetupHtbClass->bAddMarkFilter)
	{
		SYSFMT("tc filter replace dev {} parent 1: protocol ip pref 1 handle 0x{:x} fw classid 1:{}",
			SetupHtbClass->InterfaceName, SetupHtbClass->Mark, SetupHtbClass->MinorId);
	}
	return true;
}

bool ConfigurePortRouting(std::shared_ptr<WConfigurePortRouting> const& SetupPortRouting, std::string const& IfbDev)
{
	if (SetupPortRouting->bRemove)
	{
		SYSFMT("tc filter delete dev {} parent 1: protocol ip prio {} u32 match ip dport {} 0xffff flowid 1:{}", IfbDev,
			1, SetupPortRouting->Dport, SetupPortRouting->QDiscId);
		return true;
	}
	spdlog::info("Setting up ingress port routing on interface {}: dport={}, qdisc id={}", IfbDev,
		SetupPortRouting->Dport, SetupPortRouting->QDiscId);

	if (SetupPortRouting->bRemove)
	{
		SYSFMT("tc filter delete dev {} parent 1: protocol ip prio {} u32 match ip dport {} 0xffff flowid 1:{}", IfbDev,
			1, SetupPortRouting->Dport, SetupPortRouting->QDiscId);
	}
	SYSFMT("tc filter add dev {} protocol ip parent 1: prio {} u32 match ip dport {} 0xffff flowid 1:{}", IfbDev, 1,
		SetupPortRouting->Dport, SetupPortRouting->QDiscId);
	return true;
}

bool Init(std::string const& IfbDev, std::string const& IngressInterface)
{

	// Create the ifb0 interface
	SYSFMT("ip link delete {0}>/dev/null 2>&1 || ip link add {0} type ifb", IfbDev);
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
	if (Argc < 4)
	{
		spdlog::error("Usage: waechter-iplink [socket path] [ifb dev] [ingress interface]");
		return -1;
	}

	std::string SocketPath = Argv[1];
	std::string SocketSecret = Argv[2];
	std::string IfbDev = Argv[3];
	std::string IngressInterface = Argv[4];
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

	std::string Accumulator;
	while (!Handler.bStop)
	{
		WBuffer RecvBuffer;
		bool    bOk = ClientSocket->Receive(RecvBuffer);
		if (!bOk)
		{
			spdlog::info("Connection closed, exiting...");
			break;
		}

		if (RecvBuffer.GetWritePos() > 0)
		{
			Accumulator.append(RecvBuffer.GetData(), RecvBuffer.GetWritePos());
		}
		else
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			continue;
		}

		while (Accumulator.size() >= 4)
		{
			uint32_t MsgLen = 0;
			std::memcpy(&MsgLen, Accumulator.data(), 4);

			if (Accumulator.size() < 4 + MsgLen)
			{
				break;
			}

			std::string MsgData = Accumulator.substr(4, MsgLen);
			Accumulator.erase(0, 4 + MsgLen);

			std::stringstream SS(MsgData);
			WIPLinkMsg        Msg;
			try
			{
				cereal::BinaryInputArchive Iar(SS);
				Iar(Msg);
			}
			catch (std::exception const& e)
			{
				spdlog::error("Failed to deserialize message: {}", e.what());
				continue;
			}

			if (Msg.Type == EIPLinkMsgType::Exit)
			{
				spdlog::info("Received exit message, shutting down");
				Handler.bStop = true;
			}
			else if (Msg.Type == EIPLinkMsgType::SetupHtbClass && Msg.SetupHtbClass)
			{
				if (!SetupHtbClass(Msg.SetupHtbClass))
				{
					spdlog::error("Failed to setup HTB class");
				}
			}
			else if (Msg.Type == EIPLinkMsgType::ConfigurePortRouting && Msg.SetupPortRouting)
			{
				if (!ConfigurePortRouting(Msg.SetupPortRouting, IfbDev))
				{
					spdlog::error("Failed to configure port routing");
				}
			}
		}
	}

	// Cleanup
	SYSFMT2("tc filter delete dev {} parent ffff:", IngressInterface);
	SYSFMT2("tc qdisc delete dev {} root", IfbDev);
	SYSFMT2("ip link delete {}", IfbDev);
	return 0;
}