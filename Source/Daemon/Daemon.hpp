/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "Singleton.hpp"
#include "EBPF/WaechterEbpf.hpp"
#include "Communication/DaemonSocket.hpp"

class WDaemon : public TSingleton<WDaemon>
{
	WWaechterEbpf EbpfObj;

	std::shared_ptr<WDaemonSocket> DaemonSocket{};

public:
	WDaemon();
	bool        InitEbpfObj();
	bool        InitSocket();
	static void RegisterSignalHandlers();

	WWaechterEbpf& GetEbpfObj() { return EbpfObj; }

	void RunLoop();

	[[nodiscard]] std::shared_ptr<WDaemonSocket> const& GetDaemonSocket() const { return DaemonSocket; }
};
