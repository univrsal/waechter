/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "SignalHandler.hpp"

#include "spdlog/spdlog.h"

#include <csignal>

static void OnSigint(int)
{
	spdlog::info("Received exit signal");
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
	std::unique_lock Lock(SignalMutex);
	SignalCondition.wait(Lock, [this] { return bStop.load(); });
}
