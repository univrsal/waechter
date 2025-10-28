#include "DaemonClient.hpp"

#include <spdlog/spdlog.h>

#include "Messages.hpp"
#include "DaemonSocket.hpp"

void WDaemonClient::ListenThreadFunction()
{
	WBuffer Buf{};
	Buf.Resize(1024);
	while (Running)
	{
		if (!ClientSocket->Receive(Buf))
		{
			Running = false;
			break;
		}

		auto Msg = ReadFromBuffer(Buf);
		if (!Msg)
		{
			continue;
		}

		switch (Msg->GetType())
		{
			case MT_SetTcpLimit:
			{
				if (auto const TCPMsg = std::static_pointer_cast<WMessageSetTCPLimit>(Msg))
				{
					// TODO: Implement
				}
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
