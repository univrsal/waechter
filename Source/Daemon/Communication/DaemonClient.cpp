#include "DaemonClient.hpp"

#include <spdlog/spdlog.h>

#include "Messages.hpp"
#include "DaemonSocket.hpp"

void WDaemonClient::ListenThreadFunction()
{
	WBuffer Buf{};
	while (Running)
	{
		if (!ClientSocket->Receive(Buf))
		{
			Running = false;
			break;
		}

		auto Type = ReadMessageTypeFromBuffer(Buf);
		if (Type == MT_Invalid)
		{
			spdlog::warn("Received invalid message type from daemon");
			continue;
		}

		switch (Type)
		{
			case MT_SetTcpLimit:
			{
				// TODO: implement
				break;
			}
			default:
				break;
		}
	}
	ClientSocket->Close();
	spdlog::info("Client exited loop");
}

void WDaemonClient::StartListenThread()
{
	Running = true;
	ListenThread = std::thread(&WDaemonClient::ListenThreadFunction, this);
}
