#include "DaemonClient.hpp"

#include <spdlog/spdlog.h>

#include "Messages.hpp"
#include "DaemonSocket.hpp"

void WDaemonClient::ListenThreadFunction()
{
	bool    bHaveSentLimit{ false };
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
			case MT_Traffic:
			{
				if (auto const TCPMsg = std::static_pointer_cast<WMessageTraffic>(Msg))
				{
					// FIXME: Remove
					if (!bHaveSentLimit)
					{
						bHaveSentLimit = true;
						// Send<Common::WMessageSetTCPLimit>(TCPMsg->SocketFd, 1 WMiB, Common::TD_Incoming);
					}

					if (ClientPid == 0)
					{
						ClientPid = TCPMsg->Pid;
					}
				}
				break;
			}
			default:
				break;
		}
	}

	Running = false;
	// ParentSocket->GetTrafficMeter()->MarkProcessForRemoval(ClientPid);
	spdlog::info("Client disconnected");
}

void WDaemonClient::StartListenThread()
{
	Running = true;
	ListenThread = std::thread(&WDaemonClient::ListenThreadFunction, this);
}
