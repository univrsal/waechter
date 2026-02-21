/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <memory>

#include "WaechterEBPF.skel.h"
#include "EbpfObj.hpp"
#include "Types.hpp"

class WEbpfData;

enum class EEbpfInitResult
{
	Open_Failed,
	Load_Failed,
	Attach_Failed,
	Success = 0,
};

class WWaechterEbpf : public WEbpfObj
{
	std::shared_ptr<WEbpfData> Data{};
	WMsec                      QueuePileupStartTime{};

	void PrePopulatePortToPid();

public:
	waechter_ebpf* Skeleton{};

	WWaechterEbpf();
	~WWaechterEbpf();

	EEbpfInitResult Init();

	std::shared_ptr<WEbpfData> GetData() { return Data; }

	static void PrintStats();
	void        UpdateData();
};
