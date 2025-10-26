#include "DaemonSocket.hpp"

#include <filesystem>
#include <spdlog/spdlog.h>
#include <json11.hpp>

void WDaemonSocket::ListenThreadFunction()
{
	while (Running)
	{
		bool bTimedOut = false;
		if (auto ClientSocket = Socket.Accept(500, &bTimedOut))
		{
			spdlog::info("Client connected");
			auto NewClient = std::make_shared<WDaemonClient>(ClientSocket, this);
			NewClient->StartListenThread();
			ClientsMutex.lock();
			Clients.push_back(NewClient);
			ClientsMutex.unlock();
		}
		else if (Running && !bTimedOut)
		{
			// if the socket is not valid and the server is still running, print an error message
			spdlog::error("Error accepting client connection: {} ({})", strerror(errno), errno);
		}

		RemoveInactiveClients();
	}
}

bool WDaemonSocket::StartListenThread()
{
	if (std::filesystem::exists(SocketPath))
	{
		unlink(SocketPath.c_str());
	}

	if (!Socket.BindAndListen())
	{
		return false;
	}
	Running = true;
	ListenThread = std::thread(&WDaemonSocket::ListenThreadFunction, this);
	return true;
}

std::string WDaemonSocket::ToJson() const
{
	json11::Json::object TotalTraffic;
	json11::Json         Applications;

	json11::Json Data = json11::Json::object{
		{ "total", TotalTraffic },
		{ "applications", Applications },
		{ "type", "traffic" },
		{ "hostname", Hostname },
	};

	return std::move(Data.dump());
}
