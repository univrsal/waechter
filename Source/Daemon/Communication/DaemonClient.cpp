#include "DaemonClient.hpp"

#include <spdlog/spdlog.h>

#include "Messages.hpp"
#include "DaemonSocket.hpp"
#include "Rules/RuleManager.hpp"

void WDaemonClient::ListenThreadFunction()
{
	WBuffer Msg;
	while (Running)
	{
		auto Recv = ClientSocket->ReceiveFramed(Msg);
		if (Recv == 0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			continue;
		}

		if (Recv < 0)
		{
			spdlog::info("Client disconnected");
			Running = false;
			break;
		}

		auto Type = ReadMessageTypeFromBuffer(Msg);
		if (Type == MT_Invalid)
		{
			spdlog::warn("Received invalid message type from daemon client");
			continue;
		}

		switch (Type)
		{
			case MT_RuleUpdate:
				WRuleManager::GetInstance().HandleRuleChange(Msg);
				break;
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
