#include "Socket.hpp"

#include <sys/ioctl.h>
#include <spdlog/spdlog.h>

#include "ErrnoUtil.hpp"

void WClientSocket::Close()
{
	if (SocketFd < 0)
	{
		return;
	}

	close(SocketFd);
	SocketFd = -1;
	State = ES_Initial;
	Accum.Reset();
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

	if (SocketFd < 0)
	{
		if (!Open())
		{
			return false;
		}
	}

	// Set up the socket address structure
	sockaddr_un Addr{}; // value-init to zero
	Addr.sun_family = AF_UNIX;
	// Copy path and ensure null-termination
	std::strncpy(Addr.sun_path, SocketPath.c_str(), sizeof(Addr.sun_path) - 1);
	Addr.sun_path[sizeof(Addr.sun_path) - 1] = '\0';
	// Compute the correct length for AF_UNIX addresses
	auto AddrLen = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + std::strlen(Addr.sun_path) + 1);

	// Connect to the server
	if (connect(SocketFd, reinterpret_cast<struct sockaddr*>(&Addr), AddrLen) == -1)
	{
		spdlog::debug("Failed to connect to socket {}: {} ({})", SocketPath, WErrnoUtil::StrError(), errno);
		Close();
		return false;
	}
	State = ES_Connected;
	return true;
}

ssize_t WClientSocket::Send(WBuffer const& Buf)
{
	auto MarkStateForError = [&](int e) {
		if (e == EPIPE || e == EBADF || e == ECONNRESET)
		{
			State = ES_Initial;
		}
		else
		{
			spdlog::error("Socket send error: {} ({})", WErrnoUtil::StrError(), e);
		}
	};

	char const* Data = Buf.GetData();
	size_t      Total = 0;
	size_t      Len = Buf.GetWritePos();
	while (Total < Len)
	{
		ssize_t Sent = send(SocketFd, Data + Total, Len - Total, MSG_NOSIGNAL);
		if (Sent < 0)
		{
			if (errno == EINTR)
				continue; // retry
			if (errno == EPIPE)
				break; // connection closed
			if (!bBlocking && (errno == EWOULDBLOCK))
				break; // return partial in non-blocking mode
			MarkStateForError(errno);
			return (Total > 0) ? static_cast<ssize_t>(Total) : -1;
		}
		if (Sent == 0)
			break; // shouldn't happen unless disconnected
		Total += static_cast<size_t>(Sent);
	}
	return static_cast<ssize_t>(Total);
}

ssize_t WClientSocket::SendFramed(std::string const& Data)
{
	FrameBuffer.Reset();
	FrameBuffer.Write(static_cast<uint32_t>(Data.size()));
	FrameBuffer.Write(Data.data(), Data.size());
	return Send(FrameBuffer);
}

bool WClientSocket::Receive(WBuffer& Buf, bool* bDataToRead)
{
	if (bDataToRead)
	{
		*bDataToRead = false;
	}
	// Validate socket
	if (SocketFd < 0)
	{
		return false; // socket not open -> treat as ended
	}

	// Helper predicates
	auto IsConnEndedErr = [](int e) -> bool {
		return e == EBADF || e == ECONNRESET || e == ENOTCONN || e == EPIPE || e == ESHUTDOWN;
	};

	Buf.Reset();

	int Available = 0;
	if (ioctl(SocketFd, FIONREAD, &Available) < 0)
	{
		int e = errno;
		if (IsConnEndedErr(e))
		{
			State = ES_Initial;
			return false; // connection ended
		}
		// transient error or would block -> no data now, but connection still alive
		return true;
	}

	ssize_t AvailableSize = Available;

	if (AvailableSize == 0)
	{
		// Nothing to read right now. Could be EOF on some socket types; try a non-consuming recv to detect EOF.
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
			if (IsConnEndedErr(e))
			{
				State = ES_Initial;
				return false; // connection ended
			}
			// No data (would block) or transient error -> still report success but no data
			return true;
		}
		// If we peeked data, set available to the peeked amount and fall through to actual read
		AvailableSize = Result;
	}

	if (AvailableSize <= 0)
	{
		// Nothing to do, connection is still considered alive
		return true;
	}

	if (bDataToRead)
	{
		*bDataToRead = false;
	}

	Buf.Resize(static_cast<size_t>(AvailableSize));
	ssize_t Got = recv(SocketFd, Buf.GetData(), static_cast<size_t>(AvailableSize), 0);
	if (Got < 0)
	{
		int e = errno;
		if (IsConnEndedErr(e))
		{
			State = ES_Initial;
			return false; // connection ended
		}
		// would block or transient error -> keep connection alive
		return true;
	}
	if (Got == 0)
	{
		// Peer performed orderly shutdown
		State = ES_Initial;
		return false;
	}

	Buf.SetWritingPos(static_cast<size_t>(Got));
	if (bDataToRead)
	{
		*bDataToRead = true;
	}
	return true;
}