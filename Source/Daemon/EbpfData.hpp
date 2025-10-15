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

public:

	bool IsValid() const
	{
		return true;
	}

	void UpdateData()
	{
	}

	explicit WEbpfData(WWaechterEbpf const& EbpfObj);
};
