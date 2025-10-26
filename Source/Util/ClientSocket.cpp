#include "Socket.hpp"

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

	// size_t DataSize = Buf.GetWritePos();
	// auto   Result = SendFun(SocketFd, &DataSize, sizeof(DataSize), 0);
	//
	// if (!Check(Result))
	// {
	// 	return Result;
	// }

	auto Result = send(SocketFd, Buf.GetData(), Buf.GetWritePos(), 0);
	Check(Result);

	return Result;
}

bool WClientSocket::Receive(WBuffer& Buf)
{
	auto Check = [&](ssize_t Size) {
		if (Size > 0)
		{
			return true;
		}

		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			return true;
		}

		if (errno == EBADF || errno == ECONNRESET)
		{
			State = ES_Initial;
		}
		else if (errno == 0)
		{
			State = ES_ConnectedButCantSend;
		}
		else
		{
			State = ES_ConnectedButCantSend;
		}
		return false;
	};
	Buf.Reset();
	size_t DataSize = 0;
	// auto   Result = Recv(SocketFd, &DataSize, sizeof(DataSize), 0);
	//
	// if (!Check(Result))
	// {
	// 	return false;
	// }
	Buf.Resize(1024);
	auto Result = recv(SocketFd, Buf.GetData(), 1024, 0);
	if (!Check(Result))
	{
		return false;
	}
	Buf.SetWritingPos(DataSize);
	return Check(Result);
}
