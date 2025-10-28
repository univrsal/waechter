//
// Created by usr on 26/10/2025.
//

#pragma once
#include <memory>
#include <unordered_map>
#include <mutex>

#include "TrafficItem.hpp"
#include "Types.hpp"
#include "Singleton.hpp"
#include "TrafficCounter.hpp"

class WSocketInfo;
class WApplicationMap;

class WSystemMap : public TSingleton<WSystemMap>, public ITrafficItem
{
	// binary path -> application map
	std::unordered_map<std::string, std::shared_ptr<WApplicationMap>> Applications;
	std::unordered_map<WSocketCookie, std::shared_ptr<WSocketInfo>>   Sockets;

	std::string HostName{ "System" };
	// Remove exited processes and their sockets from the system map
	void Cleanup();

public:
	WSystemMap();
	~WSystemMap() override = default;

	std::mutex DataMutex;

	std::shared_ptr<WSocketInfo> MapSocket(WSocketCookie SocketCookie, WProcessId PID, bool bSilentFail = false);

	std::shared_ptr<WApplicationMap> FindOrMapApplication(std::string const& AppName);

	void RefreshAllTrafficCounters();

	void PushIncomingTraffic(WBytes Bytes, WSocketCookie SocketCookie);

	void PushOutgoingTraffic(WBytes Bytes, WSocketCookie SocketCookie);

	std::string ToJson();

	double GetDownloadSpeed() const
	{
		return GetTrafficCounter().GetDownloadSpeed();
	}

	double GetUploadSpeed() const
	{
		return GetTrafficCounter().GetUploadSpeed();
	}

	std::string UpdateJson();

	void ClearDirtyFlags();
};
