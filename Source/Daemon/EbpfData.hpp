//
// Created by usr on 12/10/2025.
//

#pragma once

#include "EbpfMap.hpp"
#include "WaechterEbpf.hpp"

struct WFlowStats
{
	unsigned long long Bytes;
	unsigned long long Packets;
};

struct WProcMeta
{
	unsigned long long PidTgId;
	unsigned long long CgroupId;
	char               Comm[16];
};

class WEbpfData
{
	WEbpfMap<WFlowStats> GlobalStats{-1};
	WEbpfMap<WFlowStats> PidStats{-1};
	WEbpfMap<WFlowStats> CgroupStats{-1};
	WEbpfMap<WFlowStats> ConnStats{-1};
	WEbpfMap<WProcMeta> CookieProcMap{-1};

public:

	bool IsValid() const
	{
		return GlobalStats.IsValid() && PidStats.IsValid() && CgroupStats.IsValid() && ConnStats.IsValid() && CookieProcMap.IsValid();
	}

	void UpdateData()
	{
		GlobalStats.Update();
		PidStats.Update();
		CgroupStats.Update();
		ConnStats.Update();
		CookieProcMap.Update();
	}

	explicit WEbpfData(WWaechterEbpf const& EbpfObj);

	WEbpfMap<WFlowStats>& GetGlobalStats() { return GlobalStats; }
	WEbpfMap<WFlowStats>& GetPidStats() { return PidStats; }
	WEbpfMap<WFlowStats>& GetCgroupStats() { return CgroupStats; }
	WEbpfMap<WFlowStats>& GetConnStats() { return ConnStats; }
	WEbpfMap<WProcMeta>& GetCookieProcMap() { return CookieProcMap; }
};
