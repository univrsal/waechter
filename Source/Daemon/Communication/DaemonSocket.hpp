#pragma once
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <algorithm>

#include "Socket.hpp"
#include "DaemonClient.hpp"

class WDaemonSocket
{
	WServerSocket Socket;

	std::atomic<bool> Running{ false };
	std::thread       ListenThread{};
	std::mutex        ClientsMutex{};
	std::string       SocketPath{};
	std::string       Hostname{};

	std::vector<std::shared_ptr<WDaemonClient>> Clients;

	void ListenThreadFunction();

public:
	explicit WDaemonSocket(std::string const& Path) : Socket(Path), SocketPath(Path)
	{
		char hostname[HOST_NAME_MAX];
		if (gethostname(hostname, HOST_NAME_MAX) == 0)
		{
			Hostname = hostname;
		}
		else
		{
			Hostname = "System";
		}
	}

	void Stop()
	{
		Running = false;
		Socket.Close();
		if (ListenThread.joinable())
		{
			ListenThread.join();
		}
	}

	~WDaemonSocket() { Stop(); }

	bool StartListenThread();

	std::vector<std::shared_ptr<WDaemonClient>>& GetClients() { return Clients; }

	void RemoveInactiveClients()
	{
		ClientsMutex.lock();
		Clients.erase(std::remove_if(Clients.begin(), Clients.end(), [](auto& Client) { return !Client->IsRunning(); }),
			Clients.end());
		ClientsMutex.unlock();
	}

	void BroadcastTrafficUpdate();
};
