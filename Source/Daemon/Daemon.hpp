/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "Singleton.hpp"
#include "MemoryStats.hpp"
#include "EBPF/WaechterEbpf.hpp"
#include "Communication/DaemonSocket.hpp"

class WDaemon : public TSingleton<WDaemon>, public IMemoryTrackable
{
	WWaechterEbpf EbpfObj;
	std::thread   EbpfPollThread{};
	std::thread   PeriodicUpdatesThread{};

	std::shared_ptr<WDaemonSocket> DaemonSocket{};

	void EbpfPollThreadFunction();

	void PeriodicUpdatesThreadFunction() const;

public:
	WDaemon();
	bool        InitEbpfObj();
	bool        InitSocket();
	static void RegisterSignalHandlers();

	WWaechterEbpf& GetEbpfObj() { return EbpfObj; }

	void RunLoop();

	[[nodiscard]] std::shared_ptr<WDaemonSocket> const& GetDaemonSocket() const { return DaemonSocket; }

	WMemoryStat GetMemoryUsage() override;
};
