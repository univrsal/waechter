#pragma once

#include <atomic>
#include <utility>
#include <thread>

#include "Socket.hpp"

class WDaemonSocket;

class WDaemonClient
{
	std::shared_ptr<WClientSocket> ClientSocket;
	WDaemonSocket*                 ParentSocket{};

	WProcessId        ClientPid{ 0 };
	std::atomic<bool> Running{ true };
	std::thread       ListenThread{};

	WBuffer SendBuffer{};

	void ListenThreadFunction();

public:
	WDaemonClient(std::shared_ptr<WClientSocket> CS, WDaemonSocket* PS)
		: ClientSocket(CS)
		, ParentSocket(PS)
	{
	}

	~WDaemonClient()
	{
		Running = false;
		ClientSocket->Close();
		if (ListenThread.joinable())
		{
			ListenThread.join();
		}
	}

	void StartListenThread();

	[[nodiscard]] WProcessId GetPid() const
	{
		return ClientPid;
	}

	[[nodiscard]] bool IsRunning() const
	{
		return Running;
	}

	[[nodiscard]] std::shared_ptr<WClientSocket> GetSocket() const
	{
		return ClientSocket;
	}

	template <class T, typename... Args>
	ssize_t Send(Args&&... Args_)
	{
		SendBuffer.Reset();
		T Msg(std::forward<Args>(Args_)...);
		Msg.Serialize(SendBuffer);
		return ClientSocket->Send(SendBuffer);
	}
};