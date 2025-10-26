//
// Created by usr on 26/10/2025.
//

#pragma once
#include <memory>
#include <unordered_map>

#include "Types.hpp"
#include "Singleton.hpp"
#include "TrafficCounter.hpp"

class WSocketInfo;
class WApplicationMap;

class WSystemMap : public TSingleton<WSystemMap>
{
	// binary path -> application map
	std::unordered_map<std::string, std::shared_ptr<WApplicationMap>> Applications;

	std::unordered_map<WSocketCookie, std::shared_ptr<WSocketInfo>> Sockets;

public:
	WTrafficCounter TrafficCounter{};

	std::shared_ptr<WSocketInfo> MapSocket(WSocketCookie SocketCookie, WProcessId PID, bool bSilentFail = false);

	std::shared_ptr<WApplicationMap> FindOrMapApplication(std::string const& AppName);

	void RefreshAllTrafficCounters();

	void PushIncomingTraffic(WBytes Bytes, WSocketCookie SocketCookie);

	void PushOutgoingTraffic(WBytes Bytes, WSocketCookie SocketCookie);
};
