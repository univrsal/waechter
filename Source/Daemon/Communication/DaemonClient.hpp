/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "Messages.hpp"

#include <atomic>
#include <utility>
#include <thread>
#include <spdlog/spdlog.h>
#include <tracy/Tracy.hpp>

// ReSharper disable CppUnusedIncludeDirective
#include <cereal/types/array.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/unordered_set.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/archives/binary.hpp>
// ReSharper restore CppUnusedIncludeDirective

#include "Socket.hpp"
#include "ErrnoUtil.hpp"
#include "Types.hpp"

class WDaemonSocket;

class WDaemonClient
{
	std::shared_ptr<WClientSocket> ClientSocket;
	WDaemonSocket*                 ParentSocket{};

	WProcessId ClientPid{ 0 };

	static void OnDataReceived(WBuffer& RecvBuf);

public:
	WDaemonClient(std::shared_ptr<WClientSocket> CS, WDaemonSocket* PS) : ClientSocket(std::move(CS)), ParentSocket(PS)
	{
		ClientSocket->OnData.connect(std::bind(&WDaemonClient::OnDataReceived, std::placeholders::_1));
		ClientSocket->OnClosed.connect([] { spdlog::info("Client disconnected"); });
		ClientSocket->StartListenThread();
	}

	~WDaemonClient() { ClientSocket->Close(); }

	[[nodiscard]] bool IsRunning() const { return ClientSocket->IsConnected(); }

	[[nodiscard]] std::shared_ptr<WClientSocket> GetSocket() const { return ClientSocket; }

	[[nodiscard]] ssize_t SendFramedData(std::string const& Data) const
	{
		ZoneScopedN("SendFramedData");
		if (!ClientSocket->IsConnected())
		{
			return 0;
		}
		return ClientSocket->SendFramed(Data);
	}

	template <class T>
	[[nodiscard]] static std::string MakeMessage(EMessageType Type, T const& Data)
	{
		std::stringstream AtlasOs{};
		{
			ZoneScopedN("SerializeMessage");
			AtlasOs << Type;
			cereal::BinaryOutputArchive AtlasArchive(AtlasOs);
			AtlasArchive(Data);
		}
		return AtlasOs.str();
	}

	template <class T>
	ssize_t SendMessage(EMessageType Type, T const& Data)
	{
		std::stringstream AtlasOs{};
		{
			ZoneScopedN("SerializeMessage");
			AtlasOs << Type;
			cereal::BinaryOutputArchive AtlasArchive(AtlasOs);
			AtlasArchive(Data);
		}
		auto Sent = SendFramedData(AtlasOs.str());
		if (Sent < 0)
		{
			spdlog::error(
				"Failed to send message of type {} to client: {}", static_cast<int>(Type), WErrnoUtil::StrError());
		}
		return Sent;
	}
};