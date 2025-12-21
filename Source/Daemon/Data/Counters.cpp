/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "Counters.hpp"

#include "MapUpdate.hpp"
#include "SystemMap.hpp"

void WSocketCounter::ProcessSocketEvent(WSocketEvent const& Event) const
{
	if (Event.EventType == NE_SocketConnect_4 && TrafficItem->ConnectionState != ESocketConnectionState::Connecting)
	{
		TrafficItem->ConnectionState = ESocketConnectionState::Connecting;
		TrafficItem->SocketTuple.Protocol = static_cast<EProtocol::Type>(Event.Data.ConnectEventData.Protocol);
		TrafficItem->SocketType |= ESocketType::Connect;
		TrafficItem->SocketTuple.RemoteEndpoint.Port = static_cast<uint16_t>(Event.Data.ConnectEventData.UserPort);
		TrafficItem->SocketTuple.RemoteEndpoint.Address.FromIPv4Uint32(Event.Data.ConnectEventData.Addr4);
	}
	else if (Event.EventType == NE_SocketConnect_6
		&& TrafficItem->ConnectionState != ESocketConnectionState::Connecting)
	{
		TrafficItem->SocketType |= ESocketType::Connect;
		TrafficItem->ConnectionState = ESocketConnectionState::Connecting;
		TrafficItem->SocketTuple.Protocol = static_cast<EProtocol::Type>(Event.Data.ConnectEventData.Protocol);
		TrafficItem->SocketTuple.RemoteEndpoint.Port = static_cast<uint16_t>(Event.Data.ConnectEventData.UserPort);
		TrafficItem->SocketTuple.RemoteEndpoint.Address.FromIPv6Array(Event.Data.ConnectEventData.Addr6);
	}
	else if (Event.EventType == NE_SocketCreate)
	{
		TrafficItem->ConnectionState = ESocketConnectionState::Created;
		TrafficItem->SocketTuple.Protocol = static_cast<EProtocol::Type>(Event.Data.SocketCreateEventData.Protocol);

		if (Event.Data.SocketCreateEventData.Family == AF_INET)
		{
			TrafficItem->SocketTuple.LocalEndpoint.Address.Family = EIPFamily::IPv4;
		}
		else if (Event.Data.SocketCreateEventData.Family == AF_INET6)
		{
			TrafficItem->SocketTuple.LocalEndpoint.Address.Family = EIPFamily::IPv6;
		}
	}
	else if (Event.EventType == NE_TCPSocketEstablished_4)
	{
		// Technically we should set the state to connected here instead of constantly setting it to connected
		// in the traffic counter
		TrafficItem->SocketTuple.LocalEndpoint.Port =
			static_cast<uint16_t>(Event.Data.TCPSocketEstablishedEventData.UserPort);
		TrafficItem->SocketTuple.LocalEndpoint.Address.FromIPv4Uint32(Event.Data.TCPSocketEstablishedEventData.Addr4);
		TrafficItem->ConnectionState = ESocketConnectionState::Connected;
	}
	else if (Event.EventType == NE_TCPSocketEstablished_6)
	{
		TrafficItem->SocketTuple.LocalEndpoint.Port =
			static_cast<uint16_t>(Event.Data.TCPSocketEstablishedEventData.UserPort);
		TrafficItem->SocketTuple.LocalEndpoint.Address.FromIPv6Array(Event.Data.TCPSocketEstablishedEventData.Addr6);
		TrafficItem->ConnectionState = ESocketConnectionState::Connected;
	}
	else if (Event.EventType == NE_SocketBind_4)
	{
		TrafficItem->SocketTuple.LocalEndpoint.Address.FromIPv4Uint32(Event.Data.SocketBindEventData.Addr4);
		TrafficItem->SocketTuple.LocalEndpoint.Port = static_cast<uint16_t>(Event.Data.SocketBindEventData.UserPort);
		TrafficItem->ConnectionState = ESocketConnectionState::Connected;
		if (Event.Data.SocketBindEventData.bImplicitBind)
		{
			TrafficItem->SocketType |= ESocketType::Connect;
		}
		else
		{
			TrafficItem->SocketType |= ESocketType::Listen;
		}
	}
	else if (Event.EventType == NE_SocketBind_6)
	{
		TrafficItem->SocketTuple.LocalEndpoint.Address.FromIPv6Array(Event.Data.SocketBindEventData.Addr6);
		TrafficItem->SocketTuple.LocalEndpoint.Port = static_cast<uint16_t>(Event.Data.SocketBindEventData.UserPort);
		TrafficItem->ConnectionState = ESocketConnectionState::Connected;
		if (Event.Data.SocketBindEventData.bImplicitBind)
		{
			TrafficItem->SocketType |= ESocketType::Connect;
		}
		else
		{
			TrafficItem->SocketType |= ESocketType::Listen;
		}
	}
	else if (Event.EventType == NE_TCPSocketListening)
	{
		TrafficItem->ConnectionState = ESocketConnectionState::Connected;
		TrafficItem->SocketType |= ESocketType::Listen;
	}
	else if (Event.EventType == NE_SocketAccept_4)
	{
		TrafficItem->ConnectionState = ESocketConnectionState::Connected;
		TrafficItem->SocketType = ESocketType::Accept;
		if (Event.Data.SocketAcceptEventData.Type == SOCK_STREAM)
		{
			TrafficItem->SocketTuple.Protocol = EProtocol::TCP;
		}
		else if (Event.Data.SocketAcceptEventData.Type == SOCK_DGRAM)
		{
			TrafficItem->SocketTuple.Protocol = EProtocol::UDP;
		}
		TrafficItem->SocketTuple.RemoteEndpoint.Port = Event.Data.SocketAcceptEventData.DestinationPort;
		TrafficItem->SocketTuple.RemoteEndpoint.Address.FromIPv4Uint32(
			Event.Data.SocketAcceptEventData.DestinationAddr4);
		TrafficItem->SocketTuple.LocalEndpoint.Port = Event.Data.SocketAcceptEventData.SourcePort;
		TrafficItem->SocketTuple.LocalEndpoint.Address.FromIPv4Uint32(
			Event.Data.SocketAcceptEventData.DestinationAddr4);
	}
	else if (Event.EventType == NE_SocketAccept_6)
	{
		TrafficItem->ConnectionState = ESocketConnectionState::Connected;
		TrafficItem->SocketType = ESocketType::Accept;
		if (Event.Data.SocketAcceptEventData.Type == SOCK_STREAM)
		{
			TrafficItem->SocketTuple.Protocol = EProtocol::TCP;
		}
		else if (Event.Data.SocketAcceptEventData.Type == SOCK_DGRAM)
		{
			TrafficItem->SocketTuple.Protocol = EProtocol::UDP;
		}
		TrafficItem->SocketTuple.RemoteEndpoint.Port = Event.Data.SocketAcceptEventData.SourcePort;
		TrafficItem->SocketTuple.RemoteEndpoint.Address.FromIPv6Array(Event.Data.SocketAcceptEventData.SourceAddr6);
		TrafficItem->SocketTuple.LocalEndpoint.Port = Event.Data.SocketAcceptEventData.DestinationPort;
		TrafficItem->SocketTuple.LocalEndpoint.Address.FromIPv6Array(Event.Data.SocketAcceptEventData.DestinationAddr6);
	}

	WSystemMap::GetInstance().GetMapUpdate().AddStateChange(
		TrafficItem->ItemId, TrafficItem->ConnectionState, TrafficItem->SocketType, TrafficItem->SocketTuple);
}
