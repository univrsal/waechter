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

	std::thread        ConnectionThread;
	mutable std::mutex SocketMutex;
	std::atomic<bool>  Running{ true };

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

	// Return whether client socket is currently connected
	[[nodiscard]] bool IsConnected() const
	{
		std::lock_guard<std::mutex> lk(SocketMutex);
		return Socket && Socket->GetState() == ES_Connected;
	}
};
