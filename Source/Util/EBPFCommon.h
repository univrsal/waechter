//
// Created by usr on 25/10/2025.
//

#pragma once

#define PACKET_HEADER_SIZE 128

enum EPacketDirection
{
	PD_Outgoing,
	PD_Incoming
};

enum ENetEventType
{
	NE_SocketCreate,
	NE_SocketConnect_4,
	NE_SocketConnect_6,
	NE_SocketAccept
};