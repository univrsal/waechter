//
// Created by usr on 27/10/2025.
//

#pragma once
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>

#include "Socket.hpp"

class WClient
{
	std::unique_ptr<WClientSocket> Socket;

	std::thread       ConnectionThread;
	std::mutex        SocketMutex;
	std::atomic<bool> Running{ true };

	void ConnectionThreadFunction();

	bool EnsureConnected();

public:
	void Stop()
	{
		Running = false;
		Socket->Close();
		if (ConnectionThread.joinable())
		{
			ConnectionThread.join();
		}
	}
	WClient();
	~WClient() { Stop(); }
};
