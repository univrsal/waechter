//
// Created by usr on 26/10/2025.
//

#include "SocketInfo.hpp"

void WSocketInfo::ProcessSocketEvent(WSocketEvent const& Event)
{
	if (Event.EventType == NE_SocketConnect_4)
	{
		SocketState = ESocketState::Connecting;
		SocketTuple.Protocol = static_cast<EProtocol::Type>(Event.Data.ConnectEventData.Protocol);
		SocketTuple.RemoteEndpoint.Port = static_cast<uint16_t>(Event.Data.ConnectEventData.UserPort);
		SocketTuple.RemoteEndpoint.Address.Family = EIPFamily::IPv4;
		SocketTuple.RemoteEndpoint.Address.Bytes[0] = static_cast<uint8_t>(Event.Data.ConnectEventData.Addr4 >> 24 & 0xFF);
		SocketTuple.RemoteEndpoint.Address.Bytes[1] = static_cast<uint8_t>(Event.Data.ConnectEventData.Addr4 >> 16 & 0xFF);
		SocketTuple.RemoteEndpoint.Address.Bytes[2] = static_cast<uint8_t>(Event.Data.ConnectEventData.Addr4 >> 8 & 0xFF);
		SocketTuple.RemoteEndpoint.Address.Bytes[3] = static_cast<uint8_t>(Event.Data.ConnectEventData.Addr4 & 0xFF);
	}
	else if (Event.EventType == NE_SocketConnect_6)
	{
		SocketState = ESocketState::Connecting;
		SocketTuple.Protocol = static_cast<EProtocol::Type>(Event.Data.ConnectEventData.Protocol);
		SocketTuple.RemoteEndpoint.Port = static_cast<uint16_t>(Event.Data.ConnectEventData.UserPort);
		SocketTuple.RemoteEndpoint.Address.Family = EIPFamily::IPv6;
		for (unsigned long i = 0; i < 4; i++)
		{
			SocketTuple.RemoteEndpoint.Address.Bytes[i * 4 + 0] = static_cast<uint8_t>(Event.Data.ConnectEventData.Addr6[i] >> 24 & 0xFF);
			SocketTuple.RemoteEndpoint.Address.Bytes[i * 4 + 1] = static_cast<uint8_t>(Event.Data.ConnectEventData.Addr6[i] >> 16 & 0xFF);
			SocketTuple.RemoteEndpoint.Address.Bytes[i * 4 + 2] = static_cast<uint8_t>(Event.Data.ConnectEventData.Addr6[i] >> 8 & 0xFF);
			SocketTuple.RemoteEndpoint.Address.Bytes[i * 4 + 3] = static_cast<uint8_t>(Event.Data.ConnectEventData.Addr6[i] & 0xFF);
		}
	}
}

void WSocketInfo::ToJson(WJson::object& Json)
{
	Json[JSON_KEY_DOWNLOAD] = GetTrafficCounter().GetDownloadSpeed();
	Json[JSON_KEY_UPLOAD] = GetTrafficCounter().GetUploadSpeed();
	Json[JSON_KEY_ID] = static_cast<double>(ItemId);
}