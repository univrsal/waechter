/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "Messages.hpp"

#include <atomic>
#include <utility>

#include "spdlog/spdlog.h"
#include "tracy/Tracy.hpp"
// ReSharper disable CppUnusedIncludeDirective
#include "cereal/types/array.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/unordered_set.hpp"
#include "cereal/types/unordered_map.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/types/string.hpp"
// (cereal/archives/binary.hpp provided via Messages.hpp)
// ReSharper restore CppUnusedIncludeDirective

#include "Socket.hpp"
#include "ErrnoUtil.hpp"
#include "Types.hpp"
#include "Communication/IClientSocket.hpp"

class WDaemonSocket;

class WDaemonClient
{
	std::shared_ptr<IClientSocket> ClientSocket;

	WProcessId ClientPid{ 0 };

	void OnDataReceived(WBuffer& RecvBuf);

	void HandleResolveRequest(WBuffer const& Buf);

public:
	explicit WDaemonClient(std::shared_ptr<IClientSocket> CS) : ClientSocket(std::move(CS))
	{
		ClientSocket->GetDataSignal().connect([this](WBuffer& Buffer) { OnDataReceived(Buffer); });
		ClientSocket->GetClosedSignal().connect([] { spdlog::info("Client disconnected"); });
		ClientSocket->StartListenThread();
	}

	~WDaemonClient() { ClientSocket->Close(); }

	[[nodiscard]] bool IsRunning() const { return ClientSocket->IsConnected(); }

	[[nodiscard]] std::shared_ptr<IClientSocket> GetSocket() const { return ClientSocket; }

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
		ZoneScopedN("SerializeMessage");
		return SerializeMessage(Type, Data);
	}

	template <class T>
	ssize_t SendMessage(EMessageType Type, T const& Data)
	{
		ZoneScopedN("SerializeMessage");
		auto Sent = SendFramedData(SerializeMessage(Type, Data));
		if (Sent < 0)
		{
			spdlog::error(
				"Failed to send message of type {} to client: {}", static_cast<int>(Type), WErrnoUtil::StrError());
		}
		return Sent;
	}
};