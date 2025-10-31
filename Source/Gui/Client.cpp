//
// Created by usr on 27/10/2025.
//

#include "Client.hpp"

#include <spdlog/spdlog.h>

#include "Messages.hpp"

void WClient::ConnectionThreadFunction()
{
	WBuffer Buf{};
	while (Running)
	{
		if (!EnsureConnected())
		{
			for (int i = 0; i < 20 && Running; ++i)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
		}
		Buf.Reset();
		bool bDataToRead{};
		if (!Socket->Receive(Buf, &bDataToRead))
		{
			Socket->Close();
			for (int i = 0; i < 20 && Running; ++i)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
			continue;
		}

		if (!bDataToRead)
		{
			for (int i = 0; i < 5 && Running; ++i)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
			continue;
		}

		auto Type = ReadMessageTypeFromBuffer(Buf);

		if (Type == MT_Invalid)
		{
			spdlog::error("Received invalid message type from server");
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		switch (Type)
		{
			case MT_TrafficTree:
			{
				TrafficTree.LoadFromBuffer(Buf);
				break;
			}
			case MT_TrafficUpdate:
			{
				break;
			}
			default:
				spdlog::warn("Received unknown message type from server: {}", static_cast<int>(Type));
				break;
		}
	}
}

bool WClient::EnsureConnected()
{
	if (Socket->GetState() == ES_Connected)
	{
		return true;
	}

	switch (Socket->GetState())
	{
		case ES_Initial:
			if (!Socket->Open())
			{
				return false;
			}
			// Fallthrough
		case ES_Opened:
			if (!Socket->Connect())
			{
				return false;
			}
			// Fallthrough
		case ES_ConnectedButCantSend:
			Socket->SetState(ES_Connected);
		default:;
	}
	return Socket->GetState() == ES_Connected;
}

WClient::WClient()
{
	Socket = std::make_unique<WClientSocket>("/var/run/waechterd.sock");
	Running = true;
	ConnectionThread = std::thread(&WClient::ConnectionThreadFunction, this);
}