/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

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
#include <fcntl.h>
#include <cereal/archives/binary.hpp>

#include "Buffer.hpp"
#include "Types.hpp"

class WSocket
{
protected:
	int         SocketFd{ -1 };
	std::string SocketPath{};

	explicit WSocket(int SocketFd_) : SocketFd(SocketFd_) {}

public:
	explicit WSocket(std::string SocketPath_) : SocketPath(std::move(SocketPath_)) {}

	[[nodiscard]] int GetFd() const { return SocketFd; }
};

class WClientSocket : public WSocket
{
	// Stores raw bytes read from the socket until a full frame is assembled
	WBuffer IncomingBuffer{ 4096 };
	bool    bIsConnected{ false };

public:
	explicit WClientSocket(int SocketFd_) : WSocket(SocketFd_), bIsConnected(true) { assert(SocketFd_ > 0); }
	explicit WClientSocket(std::string const& SocketPath_) : WSocket(SocketPath_) {}

	~WClientSocket();

	// Establishes connection if not already connected
	bool Connect();

	// Closes the socket and resets buffers
	void Close();

	[[nodiscard]] bool IsConnected() const { return bIsConnected && SocketFd >= 0; }

	// Sends raw data (no framing header)
	// Returns true if all data was sent, false on error
	bool Send(void const* Data, size_t Size);

	// Wraps data in a 4-byte length header and sends it
	bool SendFramed(std::string const& Payload);
	bool SendFramed(void const* Data, size_t Size);

	// Attempts to read from socket and extract exactly one frame.
	// Returns:
	//  true  -> A full frame was extracted into OutputBuffer.
	//  false -> Not enough data yet, or socket error (check IsConnected()).
	bool ReceiveFramed(WBuffer& OutputBuffer);

	// Standard receive into a buffer (raw bytes)
	ssize_t ReceiveRaw(void* Buffer, size_t Capacity);

	template <typename T>
	ssize_t SendMessage(T const& Message)
	{
		std::stringstream Os{};
		{
			cereal::BinaryOutputArchive Archive(Os);
			Archive(Message);
		}
		return SendFramed(Os.str());
	}
};

class WServerSocket : public WSocket
{
public:
	explicit WServerSocket(std::string const& SocketPath_) : WSocket(SocketPath_) {}

	~WServerSocket() { Close(); }

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

		if (bind(SocketFd, reinterpret_cast<sockaddr*>(&Addr), sizeof(Addr)) < 0)
		{
			return false;
		}
		if (listen(SocketFd, 5) < 0)
		{
			return false;
		}

		return true;
	}

	std::shared_ptr<WClientSocket> Accept(int TimeoutMs = -1) const
	{
		pollfd Pfd{ SocketFd, POLLIN, 0 };
		if (poll(&Pfd, 1, TimeoutMs) > 0 && (Pfd.revents & POLLIN))
		{
			int ClientFd = accept(SocketFd, nullptr, nullptr);
			if (ClientFd >= 0)
			{
				return std::make_shared<WClientSocket>(ClientFd);
			}
		}
		return nullptr;
	}
};
