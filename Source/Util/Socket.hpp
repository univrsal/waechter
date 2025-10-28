#pragma once

#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <iostream>
#include <memory>
#include <cassert>
#include <utility>
#include <atomic>

#include "Buffer.hpp"
#include "Types.hpp"

#include <fcntl.h>

class WSocket
{
protected:
	int         SocketFd{ -1 };
	std::string SocketPath{};

	explicit WSocket(int SocketFd_)
		: SocketFd(SocketFd_)
	{
	}

public:
	explicit WSocket(std::string SocketPath_)
		: SocketPath(std::move(SocketPath_))
	{
	}

	[[nodiscard]] int GetFd() const
	{
		return SocketFd;
	}
};

enum ESocketState
{
	ES_Initial,
	ES_Opened,
	ES_Connected,
	ES_ConnectedButCantSend
};

class WClientSocket : public WSocket
{
	std::atomic<ESocketState> State{};
	bool                      bBlocking{ true };

public:
	explicit WClientSocket(int SocketFd_)
		: WSocket(SocketFd_)
	{
		assert(SocketFd_ > 0);
	}

	explicit WClientSocket(std::string const& SocketPath_)
		: WSocket(SocketPath_)
	{
	}

	~WClientSocket()
	{
		Close();
	}

	void Close();

	bool Open();

	bool Connect();

	ssize_t Send(WBuffer const& Buf);

	bool Receive(WBuffer& Buf, bool* bDataToRead = nullptr);

	[[nodiscard]] ESocketState GetState() const
	{
		return State.load(std::memory_order_acquire);
	}

	void SetState(ESocketState NewState)
	{
		State.store(NewState, std::memory_order_release);
	}

	void SetNonBlocking(bool bNonBlocking)
	{
		int Flags = fcntl(SocketFd, F_GETFL, 0);
		if (bNonBlocking)
		{
			Flags |= O_NONBLOCK;
		}
		else
		{
			Flags &= ~O_NONBLOCK;
		}
		fcntl(SocketFd, F_SETFL, Flags);
		bBlocking = !bNonBlocking;
	}
};

class WServerSocket : public WSocket
{
public:
	explicit WServerSocket(std::string const& SocketPath_)
		: WSocket(SocketPath_)
	{
	}

	~WServerSocket()
	{
		Close();
	}

	void Close()
	{
		if (SocketFd < 0)
		{
			return;
		}

		close(SocketFd);
		unlink(SocketPath.c_str());
		SocketFd = -1;
	}

	bool BindAndListen()
	{
		SocketFd = socket(AF_UNIX, SOCK_STREAM, 0);
		if (SocketFd < 0)
		{
			return false;
		}

		sockaddr_un Addr{};
		Addr.sun_family = AF_UNIX;
		strncpy(Addr.sun_path, SocketPath.c_str(), sizeof(Addr.sun_path) - 1);

		if (bind(SocketFd, (sockaddr*)&Addr, sizeof(Addr)) < 0)
		{
			return false;
		}

		if (listen(SocketFd, 5) < 0)
		{
			return false;
		}

		return true;
	}

	std::shared_ptr<WClientSocket> Accept(int TimeoutMs = -1, bool* bTimedOut = nullptr) const
	{
		pollfd pfd{};
		pfd.fd = SocketFd;
		pfd.events = POLLIN;

		if (int const Ret = poll(&pfd, 1, TimeoutMs); Ret > 0)
		{
			if (pfd.revents & POLLIN)
			{
				int ClientFd = accept(SocketFd, nullptr, nullptr);
				if (ClientFd < 0)
				{
					return nullptr;
				}
				return std::make_shared<WClientSocket>(ClientFd);
			}
		}
		else if (Ret == 0 && bTimedOut)
		{
			*bTimedOut = true;
		}
		return nullptr;
	}
};
