//
// Created by usr on 27/10/2025.
//

#pragma once
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>

#include "Socket.hpp"
#include "Windows/TrafficTree.hpp"
#include "TrafficCounter.hpp"

struct WClientItem : ITrafficItem
{
	[[nodiscard]] ETrafficItemType GetType() const override { return TI_Application; }
};

class WClient
{
	std::unique_ptr<WClientSocket> Socket;

	std::thread        ConnectionThread;
	mutable std::mutex SocketMutex;
	std::atomic<bool>  Running{ true };
	WTrafficTree       TrafficTree{};

	TTrafficCounter<WClientItem> TrafficCounter{ std::make_shared<WClientItem>() };

	void ConnectionThreadFunction();

	bool EnsureConnected();

public:
	std::atomic<WBytesPerSecond> DaemonToClientTrafficRate{ 0 };
	std::atomic<WBytesPerSecond> ClientToDaemonTrafficRate{ 0 };

	void Start()
	{
		Socket = std::make_unique<WClientSocket>("/var/run/waechterd.sock");
		Running = true;
		ConnectionThread = std::thread(&WClient::ConnectionThreadFunction, this);
	}

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

	WTrafficTree& GetTrafficTree() { return TrafficTree; }
};
