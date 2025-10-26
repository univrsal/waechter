//
// Created by usr on 26/10/2025.
//

#include "ProcessMap.hpp"
#include "SocketInfo.hpp"

std::shared_ptr<WSocketInfo> WProcessMap::FindOrMapSocket(WSocketCookie Cookie)
{
	auto It = Sockets.find(Cookie);
	if (It != Sockets.end())
	{
		return It->second;
	}

	auto SocketInfo = std::make_shared<WSocketInfo>();
	Sockets.emplace(Cookie, SocketInfo);
	return SocketInfo;
}