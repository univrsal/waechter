/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "SignalHandler.hpp"

#include <csignal>

static void OnSigint(int)
{
	auto& Handler = WSignalHandler::GetInstance();
	Handler.bStop = true;
	Handler.SignalCondition.notify_all();
}

WSignalHandler::WSignalHandler()
{
	signal(SIGINT, OnSigint);
	signal(SIGTERM, OnSigint);
}

void WSignalHandler::Wait()
{
	std::unique_lock<std::mutex> Lock(SignalMutex);
	SignalCondition.wait(Lock, [this] { return bStop.load(); });
}
