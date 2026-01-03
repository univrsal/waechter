/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "Socket.hpp"
#include <spdlog/spdlog.h>
#include <vector>

WClientSocket::~WClientSocket()
{
	Close();
}

void WClientSocket::Close()
{
	if (SocketFd >= 0)
	{
		close(SocketFd);
		SocketFd = -1;
	}
	bIsConnected = false;
	IncomingBuffer.Reset();
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
		return false;

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
	uint32_t Length = static_cast<uint32_t>(Size);
	if (!Send(&Length, sizeof(Length)))
		return false;

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

	// 1. Read available data from network into our internal accumulator
	// We read a chunk at a time.
	char    IoBuf[4096];
	ssize_t BytesRead = ReceiveRaw(IoBuf, sizeof(IoBuf));

	if (BytesRead > 0)
	{
		IncomingBuffer.Write(IoBuf, static_cast<size_t>(BytesRead));
	}
	// If BytesRead < 0 (error) or == 0 (closed), IsConnected() will likely be false now,
	// but we might still have data in IncomingBuffer to process, so we continue.

	// 2. Do we have enough for a header?
	if (IncomingBuffer.GetReadableSize() < sizeof(uint32_t))
	{
		return false;
	}

	// 3. Peek the length
	uint32_t FrameLen = 0;
	std::memcpy(&FrameLen, IncomingBuffer.PeekReadPtr(), sizeof(uint32_t));

	// 4. Do we have the full body?
	if (IncomingBuffer.GetReadableSize() < sizeof(uint32_t) + FrameLen)
	{
		// Not enough data yet, wait for next call
		// Optional: Compact here if buffer is getting full but fragmented
		if (IncomingBuffer.GetSize() > 8192 && IncomingBuffer.GetReadPos() > 1024)
		{
			IncomingBuffer.Compact();
		}
		return false;
	}

	// 5. We have a full frame. Extract it.
	IncomingBuffer.Consume(sizeof(uint32_t)); // Skip header

	OutputBuffer.Reset();
	// Ensure space and copy directly
	// Note: WBuffer::Write ensures capacity
	OutputBuffer.Write(IncomingBuffer.PeekReadPtr(), FrameLen);

	IncomingBuffer.Consume(FrameLen);

	// Optional: Auto-compact to keep memory usage low over time
	if (IncomingBuffer.GetReadPos() > 2048)
	{
		IncomingBuffer.Compact();
	}

	return true;
}