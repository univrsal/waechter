//
// Created by usr on 09/10/2025.
//

#include <thread>

#include <spdlog/spdlog.h>

#include "SignalHandler.hpp"
#include "DaemonConfig.hpp"
#include "EbpfObj.hpp"
#include "ErrnoUtil.hpp"

#include <bpf/libbpf_legacy.h>
#include <linux/if_link.h>
#include <net/if.h>

inline int InitEbpfObj(WEbpfObj& EbpfObj, int IfIndex)
{
	if (!EbpfObj)
	{
		spdlog::critical("failed to load epf object");
		return -1;
	}

	if (!EbpfObj.Load())
	{
		spdlog::critical("failed to load ebpf object");
		return -1;
	}

	if (!EbpfObj.FindAndAttachXdpProgram("xdp_waechter", IfIndex, XDP_FLAGS_UPDATE_IF_NOEXIST | XDP_FLAGS_SKB_MODE))
	{
		spdlog::critical("failed to attach xdp program");
		return -1;
	}

	if (!EbpfObj.FindAndAttachProgram("cgskb_ingress", BPF_CGROUP_INET_INGRESS))
	{
		spdlog::critical("failed to attach cgroup egress program");
		return -1;
	}

	if (!EbpfObj.FindAndAttachProgram("cgskb_egress", BPF_CGROUP_INET_EGRESS))
	{
		spdlog::critical("failed to attach cgroup ingress program");
		return -1;
	}
	return 0;
}

int main()
{
	WSignalHandler& SignalHandler = WSignalHandler::GetInstance();

	spdlog::info("Waechter daemon starting");

	libbpf_set_strict_mode(LIBBPF_STRICT_ALL);

	WDaemonConfig::GetInstance().LogConfig();

	int IfIndex = if_nametoindex(WDaemonConfig::GetInstance().NetworkInterfaceName.c_str());

	if (!IfIndex)
	{
		spdlog::critical("Network interface name could not be determined: {}", WErrnoUtil::StrError());
		return -1;
	}

	WEbpfObj EbpfObj("ebpf/waechter_bpf.o");

	if (InitEbpfObj(EbpfObj, IfIndex) != 0)
	{
		return -1;
	}

	while (!SignalHandler.bStop)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	spdlog::info("Waechter daemon stopped");
	return 0;
}