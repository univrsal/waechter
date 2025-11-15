#pragma once
#include "Messages.hpp"

#include <atomic>
#include <utility>
#include <thread>
#include <spdlog/spdlog.h>

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

class WDaemonSocket;

class WDaemonClient
{
	std::shared_ptr<WClientSocket> ClientSocket;
	WDaemonSocket*                 ParentSocket{};

	WProcessId        ClientPid{ 0 };
	std::atomic<bool> Running{ true };
	std::thread       ListenThread{};

	WBuffer SendBuffer{};

	void ListenThreadFunction();

public:
	WDaemonClient(std::shared_ptr<WClientSocket> CS, WDaemonSocket* PS) : ClientSocket(std::move(CS)), ParentSocket(PS)
	{
	}

	~WDaemonClient()
	{
		Running = false;
		ClientSocket->Close();
		if (ListenThread.joinable())
		{
			ListenThread.join();
		}
	}

	void StartListenThread();

	[[nodiscard]] WProcessId GetPid() const { return ClientPid; }

	[[nodiscard]] bool IsRunning() const { return Running; }

	[[nodiscard]] std::shared_ptr<WClientSocket> GetSocket() const { return ClientSocket; }

	ssize_t SendData(WBuffer& Buffer) const { return ClientSocket->Send(Buffer); }

	ssize_t SendFramedData(std::string const& Data) const { return ClientSocket->SendFramed(Data); }

	template <class T>
	void SendMessage(EMessageType Type, T const& Data)
	{
		std::stringstream AtlasOs{};
		{
			AtlasOs << Type;
			cereal::BinaryOutputArchive AtlasArchive(AtlasOs);
			AtlasArchive(Data);
		}
		auto Sent = SendFramedData(AtlasOs.str());
		if (Sent < 0)
		{
			spdlog::error("Failed to send app icon atlas data to client");
		}
		else
		{
			spdlog::info("Sent app icon atlas data ({} bytes) to client", Sent);
		}
	}
};