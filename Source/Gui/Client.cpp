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
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			continue;
		}

		if (!bDataToRead)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
			continue;
		}

		auto Msg = ReadFromBuffer(Buf);

		if (!Msg)
		{
			spdlog::error("Failed to parse message from server");
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		switch (Msg->GetType())
		{
			case MT_TrafficTree:
			{
				if (auto const TrafficMsg = std::static_pointer_cast<WMessageTrafficTree>(Msg))
				{
					TrafficTree.LoadFromJson(TrafficMsg->Json);
				}
				break;
			}
			case MT_TrafficUpdate:
			{
				if (auto const TrafficMsg = std::static_pointer_cast<WMessageTrafficUpdate>(Msg))
				{
					TrafficTree.UpdateFromJson(TrafficMsg->Json);
				}
				break;
			}
			default:
				spdlog::warn("Received unknown message type from server: {}", static_cast<int>(Msg->GetType()));
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
	Socket = std::make_unique<WClientSocket>("/home/usr/docs/git/cpp/waechter/cmake-build-debug/Source/waechterd.sock");
	Running = true;
	ConnectionThread = std::thread(&WClient::ConnectionThreadFunction, this);
}