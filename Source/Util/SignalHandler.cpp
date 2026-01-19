/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "SignalHandler.hpp"

#include <csignal>

static void OnSigint(int)
{
	WSignalHandler::GetInstance().bStop = true;
}

WSignalHandler::WSignalHandler()
{
	signal(SIGINT, OnSigint);
	signal(SIGTERM, OnSigint);
}