/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "UnixSocketSource.hpp"

#include "Util/Settings.hpp"
#include "Time.hpp"

WUnixSocketSource::WUnixSocketSource()
{
	Socket = std::make_shared<WClientSocket>(WSettings::GetInstance().SocketPath);
}

void WUnixSocketSource::Start()
{
	Socket->StartListenThread();

	WTimerManager::GetInstance().AddTimer(0.5, [this] {
		if (!Socket->IsConnected())
		{
			Socket->Connect();
			if (Socket->IsConnected())
			{
				Socket->StartListenThread();
			}
		}
	});
}