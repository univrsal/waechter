//
// Created by usr on 26/10/2025.
//

#include "ProcessMap.hpp"
#include "SocketInfo.hpp"

void WProcessMap::RefreshAllTrafficCounters()
{
	TrafficCounter.Refresh();

	for (auto& [Cookie, SocketInfo] : Sockets)
	{
		SocketInfo->TrafficCounter.Refresh();
	}
}

std::shared_ptr<WSocketInfo> WProcessMap::FindOrMapSocket(WSocketCookie Cookie)
{
	auto It = Sockets.find(Cookie);
	if (It != Sockets.end())
	{
		return It->second;
	}

	auto SocketInfo = std::make_shared<WSocketInfo>();
	SocketInfo->ParentProcess = this;
	Sockets.emplace(Cookie, SocketInfo);
	return SocketInfo;
}

void WProcessMap::ToJson(WJson::object& Json)
{
	Json[JSON_KEY_PID] = PID;
	Json[JSON_KEY_CMDLINE] = CmdLine;

	WJson::array SocketsArray;

	for (auto& [Cookie, SocketInfo] : Sockets)
	{
		WJson::object SocketJson;
		SocketInfo->ToJson(SocketJson);
		SocketJson[JSON_KEY_SOCKET_COOKIE] = static_cast<double>(Cookie);
		SocketsArray.emplace_back(SocketJson);
	}

	Json[JSON_KEY_SOCKETS] = SocketsArray;
}