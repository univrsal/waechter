//
// Created by usr on 27/10/2025.
//

#pragma once
#include "Messages.hpp"

#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
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

#include "ErrnoUtil.hpp"
#include "Socket.hpp"
#include "Windows/TrafficTree.hpp"
#include "TrafficCounter.hpp"
#include "Singleton.hpp"
#include "Data/TrafficItem.hpp"

struct WClientItem : ITrafficItem
{
	[[nodiscard]] ETrafficItemType GetType() const override { return TI_Application; }
};

class WClient : public TSingleton<WClient>
{
	std::shared_ptr<WClientSocket> Socket;

	std::thread                   ConnectionThread;
	mutable std::mutex            SocketMutex;
	std::atomic<bool>             Running{ true };
	std::shared_ptr<WTrafficTree> TrafficTree{};

	TTrafficCounter<WClientItem> TrafficCounter{ std::make_shared<WClientItem>() };

	void ConnectionThreadFunction();

	bool EnsureConnected();

public:
	std::atomic<WBytesPerSecond> DaemonToClientTrafficRate{ 0 };
	std::atomic<WBytesPerSecond> ClientToDaemonTrafficRate{ 0 };

	void Start();

	void Stop()
	{
		Running = false;
		Socket->Close();
		if (ConnectionThread.joinable())
		{
			ConnectionThread.join();
		}
	}
	WClient();
	~WClient() { Stop(); }

	// Return whether client socket is currently connected
	[[nodiscard]] bool IsConnected() const
	{
		std::lock_guard lk(SocketMutex);
		return Socket && Socket->GetState() == ES_Connected;
	}

	std::shared_ptr<WTrafficTree> GetTrafficTree() { return TrafficTree; }

	ssize_t SendFramedData(std::string const& Data) const { return Socket->SendFramed(Data); }

	template <class T>
	void SendMessage(EMessageType Type, T const& Data) const
	{
		std::stringstream AtlasOs{};
		{
			AtlasOs << Type;
			cereal::BinaryOutputArchive AtlasArchive(AtlasOs);
			AtlasArchive(Data);
		}
		if (auto Sent = SendFramedData(AtlasOs.str()); Sent < 0)
		{
			spdlog::error(
				"Failed to send message of type {} to daemon: {}", static_cast<int>(Type), WErrnoUtil::StrError());
		}
	}
};
