//
// Created by usr on 12/10/2025.
//

#include "EbpfData.hpp"

WEbpfData::WEbpfData(WWaechterEbpf const& EbpfObj)
{
	GlobalStats = WEbpfMap<WFlowStats>(EbpfObj.FindMapFd("global_stats"));
	PidStats = WEbpfMap<WFlowStats>(EbpfObj.FindMapFd("pid_stats"));
	CgroupStats = WEbpfMap<WFlowStats>(EbpfObj.FindMapFd("cgroup_stats"));
	ConnStats = WEbpfMap<WFlowStats>(EbpfObj.FindMapFd("conn_stats"));
	CookieProcMap = WEbpfMap<WProcMeta>(EbpfObj.FindMapFd("cookie_proc_map"));
}