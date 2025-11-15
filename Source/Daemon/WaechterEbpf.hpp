//
// Created by usr on 12/10/2025.
//

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
	int                        InterfaceIndex{ -1 };
	std::shared_ptr<WEbpfData> Data{};
	WMsec                      QueuePileupStartTime{};

public:
	waechter_ebpf* Skeleton{};

	WWaechterEbpf();
	~WWaechterEbpf();

	EEbpfInitResult Init();

	std::shared_ptr<WEbpfData> GetData() { return Data; }

	static void PrintStats();
	void        UpdateData();
};
