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
		bool bDataToRead;
		bool bOk = ClientSocket->Receive(Msg, &bDataToRead);
		if (!bDataToRead)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			continue;
		}

		if (!bOk)
		{
			spdlog::info("Client disconnected");
			Running = false;
			break;
		}

		Msg.Consume(4); // Skip message length for now
		auto Type = ReadMessageTypeFromBuffer(Msg);
		if (Type == MT_Invalid)
		{
			spdlog::warn("Received invalid message type from daemon client");
			continue;
		}
		spdlog::info("Received message: {}", static_cast<int>(Type));

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
