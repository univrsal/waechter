//
// Created by usr on 19/11/2025.
//

#pragma once
#include "Singleton.hpp"
#include "WaechterEbpf.hpp"
#include "Communication/DaemonSocket.hpp"

class WDaemon : public TSingleton<WDaemon>
{
	WWaechterEbpf EbpfObj;

	std::shared_ptr<WDaemonSocket> DaemonSocket{};

public:
	WDaemon();
	bool InitEbpfObj();
	bool InitSocket();

	void RunLoop();

	std::shared_ptr<WDaemonSocket> const& GetDaemonSocket() const { return DaemonSocket; }
};
