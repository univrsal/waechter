/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <csignal>

#include "Singleton.hpp"

class WSignalHandler : public TSingleton<WSignalHandler>
{
public:
	WSignalHandler();

	void Wait();

	std::atomic<bool>       bStop{ false };
	std::condition_variable SignalCondition;
	std::mutex              SignalMutex;

	static void Breakpoint()
	{
#if _WIN32
		__debugbreak();
#else
		raise(SIGTRAP);
#endif
	}
};
