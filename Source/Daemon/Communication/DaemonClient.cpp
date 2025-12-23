/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "DaemonClient.hpp"

#include <spdlog/spdlog.h>

#include "Messages.hpp"
#include "DaemonSocket.hpp"
#include "Rules/RuleManager.hpp"

void WDaemonClient::ListenThreadFunction()
{
	WBuffer Accumulator;
	WBuffer RecvBuf;
	while (Running)
	{
		bool bDataToRead;
		bool bOk = ClientSocket->Receive(RecvBuf, &bDataToRead);

		if (!bOk)
		{
			Running = false;
			break;
		}

		if (!bDataToRead)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			continue;
		}

		Accumulator.Write(RecvBuf.GetData(), RecvBuf.GetWritePos());

		while (Accumulator.GetReadableSize() >= 4)
		{
			uint32_t PayloadLen = 0;
			std::memcpy(&PayloadLen, Accumulator.PeekReadPtr(), 4);

			if (Accumulator.GetReadableSize() < 4 + PayloadLen)
			{
				break;
			}

			// Extract the full message
			std::vector<char> RawMsg(4 + PayloadLen);
			Accumulator.Read(RawMsg.data(), 4 + PayloadLen);

			WBuffer Msg(RawMsg.size());
			Msg.Write(RawMsg.data(), RawMsg.size());

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
		Accumulator.Compact();
	}
	ClientSocket->Close();
	spdlog::info("Client disconnected");
}

void WDaemonClient::StartListenThread()
{
	Running = true;
	ListenThread = std::thread(&WDaemonClient::ListenThreadFunction, this);
}
