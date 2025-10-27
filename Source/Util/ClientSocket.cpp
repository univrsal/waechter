#include "Socket.hpp"
#include "../../Thirdparty/spdlog/include/spdlog/spdlog.h"
#include <sys/ioctl.h>

void WClientSocket::Close()
{
	if (SocketFd < 0)
	{
		return;
	}

	close(SocketFd);
	SocketFd = -1;
}

bool WClientSocket::Open()
{
	if (State == ES_Opened)
	{
		return true;
	}

	SocketFd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (SocketFd < 0)
	{
		return false;
	}
	State = ES_Opened;
	return true;
}

bool WClientSocket::Connect()
{
	if (State == ES_Connected)
	{
		return true;
	}

	// Set up the socket address structure
	sockaddr_un Addr{};
	Addr.sun_family = AF_UNIX;
	strncpy(Addr.sun_path, SocketPath.c_str(), sizeof(Addr.sun_path) - 1);

	// Connect to the server
	if (connect(SocketFd, reinterpret_cast<struct sockaddr*>(&Addr), sizeof(Addr)) == -1)
	{
		return false;
	}
	State = ES_Connected;
	return true;
}

ssize_t WClientSocket::Send(WBuffer const& Buf)
{
	auto Check = [&](ssize_t Result) {
		if (Result < 0)
		{
			if (errno == EPIPE || errno == EBADF || errno == ECONNRESET)
			{
				State = ES_Initial;
			}
			else
			{
				State = ES_ConnectedButCantSend;
			}
			return false;
		}
		return true;
	};

	auto Result = send(SocketFd, Buf.GetData(), Buf.GetWritePos(), 0);
	Check(Result);

	spdlog::info("state: {}, sent {} bytes", static_cast<int>(State), Result);

	return Result;
}

bool WClientSocket::Receive(WBuffer& Buf)
{
	// Validate socket
	if (SocketFd < 0)
	{
		return false;
	}

	auto HandleErrorState = [&](int SavedErrno) -> void {
		// No data available right now
		if (SavedErrno == EAGAIN)
			return;
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
		if (SavedErrno == EWOULDBLOCK)
			return;
#endif
		if (SavedErrno == EBADF || SavedErrno == ECONNRESET)
		{
			State = ES_Initial;
			return;
		}
		// Other errors: mark as can't send/receive
		State = ES_ConnectedButCantSend;
	};

	Buf.Reset();

	int Available = 0;
	if (ioctl(SocketFd, FIONREAD, &Available) < 0)
	{
		int e = errno;
		HandleErrorState(e);
		return false;
	}

	ssize_t AvailableSize = Available;

	if (AvailableSize == 0)
	{
		// Nothing to read right now. Could be EOF on some socket types; try a non-consuming recv to detect EOF.
		// Use peek with 1 byte to detect EOF (recv returns 0) or no-data (would block / return -1 with EAGAIN).
		char    PeekBuf;
		ssize_t Result = recv(SocketFd, &PeekBuf, 1, MSG_PEEK | MSG_DONTWAIT);
		if (Result == 0)
		{
			// orderly shutdown / EOF
			State = ES_Initial;
			return false;
		}
		if (Result < 0)
		{
			int e = errno;
			// EAGAIN/EWOULDBLOCK: no data right now
			HandleErrorState(e);
			// If the error was EAGAIN/EWOULDBLOCK, handler returns without changing state; still return false.
			return false;
		}
		// If we peeked data, set available to at least 1 and fall through to actual read
		AvailableSize = Result;
	}

	if (AvailableSize <= 0)
	{
		return false;
	}

	Buf.Resize(static_cast<size_t>(AvailableSize));
	ssize_t Got = recv(SocketFd, Buf.GetData(), static_cast<size_t>(AvailableSize), 0);
	if (Got < 0)
	{
		int e = errno;
		HandleErrorState(e);
		return false;
	}
	if (Got == 0)
	{
		// Peer performed orderly shutdown
		State = ES_Initial;
		return false;
	}

	Buf.SetWritingPos(static_cast<size_t>(Got));
	return true;
}
