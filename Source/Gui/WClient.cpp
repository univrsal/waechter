//
// Created by usr on 27/10/2025.
//

#include "WClient.hpp"

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

		if (!Socket->Receive(Buf))
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
					// TODO: Parse tree
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