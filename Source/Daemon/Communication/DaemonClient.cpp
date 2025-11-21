#include "DaemonClient.hpp"

#include <spdlog/spdlog.h>

#include "Messages.hpp"
#include "DaemonSocket.hpp"
#include "Rules/RuleManager.hpp"

void WDaemonClient::ListenThreadFunction()
{
	WBuffer Buf{};
	bool    bDataToRead{};
	WBuffer Accum{};
	WBuffer Msg;
	while (Running)
	{
		if (!ClientSocket->Receive(Buf, &bDataToRead))
		{
			Running = false;
			break;
		}

		if (!bDataToRead)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			continue;
		}

		// Append to accumulator and parse framed messages (size-prefixed)
		Accum.Write(Buf.GetData(), Buf.GetWritePos());
		for (;;)
		{
			if (Accum.GetReadableSize() < 4)
				break;
			uint32_t FrameLength = 0;
			std::memcpy(&FrameLength, Accum.PeekReadPtr(), 4);
			if (Accum.GetReadableSize() < static_cast<size_t>(4 + FrameLength))
			{
				break;
			}
			Accum.Consume(4);
			Msg.Reset(); // reuse buffer per frame to reset read/write cursors
			Msg.Resize(FrameLength);
			std::memcpy(Msg.GetData(), Accum.PeekReadPtr(), FrameLength);
			Msg.SetWritingPos(FrameLength);
			Accum.Consume(FrameLength);

			auto Type = ReadMessageTypeFromBuffer(Msg);
			if (Type == MT_Invalid)
			{
				spdlog::warn("Received invalid message type from daemon client");
				continue;
			}

			switch (Type)
			{
				case MT_SetTcpLimit:
				{
					// TODO: implement
				}
				break;
				case MT_RuleUpdate:
					WRuleManager::GetInstance().HandleRuleChange(Msg);
					break;
				default:
					break;
			}
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
