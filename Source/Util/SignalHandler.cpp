//
// Created by usr on 09/10/2025.
//

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