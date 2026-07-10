/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "Socket.hpp"

#include "spdlog/spdlog.h"

#include <netinet/in.h>

void WClientSocket::ListenThreadFunction()
{
	WBuffer RecvBuf;
	while (bListenThreadRunning && IsConnected())
	{
		while (bListenThreadRunning && IsConnected() && ReceiveFramed(RecvBuf))
		{
			OnData(RecvBuf);
			RecvBuf.Reset();
		}
	}
	bListenThreadRunning = false;
}

WClientSocket::~WClientSocket()
{
	Close();
}

void WClientSocket::Close()
{
	bool bWasConnected = bIsConnected;
	bListenThreadRunning = false;
	bIsConnected = false;
	if (SocketFd >= 0)
	{
		shutdown(SocketFd, SHUT_RDWR);
		close(SocketFd);
		SocketFd = -1;
	}

	if (std::this_thread::get_id() == ListenerThread.get_id())
	{
		// We're in the listener thread, cannot join ourselves
		// Just let the thread finish naturally.
	}
	else
	{
		if (ListenerThread.joinable())
		{
			ListenerThread.join();
		}
	}
	if (bWasConnected)
	{
		OnClosed();
	}
}

bool WClientSocket::Connect()
{
	if (IsConnected())
	{
		return true;
	}

	SocketFd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (SocketFd < 0)
	{
		return false;
	}

	sockaddr_un Addr{};
	Addr.sun_family = AF_UNIX;
	std::strncpy(Addr.sun_path, SocketPath.c_str(), sizeof(Addr.sun_path) - 1);

	// Connect
	if (connect(SocketFd, reinterpret_cast<struct sockaddr*>(&Addr), sizeof(Addr)) == -1)
	{
		spdlog::debug("Failed to connect to socket {}", SocketPath);
		Close();
		return false;
	}

	bIsConnected = true;
	return true;
}

bool WClientSocket::Send(void const* Data, size_t Size)
{
	if (!IsConnected() || Size == 0)
	{
		return false;
	}

	char const* Ptr = static_cast<char const*>(Data);
	size_t      TotalSent = 0;

	while (TotalSent < Size)
	{
		ssize_t Sent = send(SocketFd, Ptr + TotalSent, Size - TotalSent, MSG_NOSIGNAL);
		if (Sent < 0)
		{
			if (errno == EINTR)
				continue;
			Close();
			return false;
		}
		TotalSent += static_cast<size_t>(Sent);
	}
	return true;
}

bool WClientSocket::SendFramed(void const* Data, size_t Size)
{
	// 1. Send Length Header (uint32)
	auto Length = static_cast<uint32_t>(Size);
	if (!Send(&Length, sizeof(Length)))
	{
		return false;
	}

	// 2. Send Body
	return Send(Data, Size);
}

bool WClientSocket::SendFramed(std::string const& Payload)
{
	return SendFramed(Payload.data(), Payload.size());
}

ssize_t WClientSocket::ReceiveRaw(void* Buffer, size_t Capacity)
{
	if (!IsConnected())
		return -1;
	ssize_t Bytes = recv(SocketFd, Buffer, Capacity, 0);

	if (Bytes == 0)
	{
		// Orderly shutdown
		Close();
	}
	else if (Bytes < 0)
	{
		if (errno != EINTR && errno != EWOULDBLOCK && errno != EAGAIN)
		{
			Close();
		}
	}
	return Bytes;
}

bool WClientSocket::ReceiveFramed(WBuffer& OutputBuffer)
{
	if (!IsConnected())
	{
		return false;
	}

	if (TryPopBufferedFrame(OutputBuffer))
	{
		return true;
	}

	static char   IoBuf[4096];
	ssize_t const BytesRead = ReceiveRaw(IoBuf, sizeof(IoBuf));
	if (BytesRead <= 0)
	{
		return false;
	}

	// Accumulate data (handle fragmented messages)
	IncomingBuffer.insert(IncomingBuffer.end(), IoBuf, IoBuf + BytesRead);
	return TryPopBufferedFrame(OutputBuffer);
}

bool WClientSocket::TryPopBufferedFrame(WBuffer& OutputBuffer)
{
	// Try to extract complete frames
	while (IncomingBuffer.size() >= sizeof(uint32_t))
	{
		uint32_t FrameLength = 0;
		std::memcpy(&FrameLength, IncomingBuffer.data(), sizeof(uint32_t));

		if (IncomingBuffer.size() < sizeof(uint32_t) + FrameLength)
		{
			// Not enough data for complete frame
			return false;
		}

		// Extract frame data (skip length prefix)
		OutputBuffer.Reset();
		OutputBuffer.Write(std::span<char const>(IncomingBuffer.data() + sizeof(uint32_t), FrameLength));

		// Remove processed data from buffer
		IncomingBuffer.erase(
			IncomingBuffer.begin(), IncomingBuffer.begin() + static_cast<ptrdiff_t>(sizeof(uint32_t) + FrameLength));
		return true;
	}
	return false;
}