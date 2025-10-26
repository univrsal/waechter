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

#define WUNUSED(x) (void)(x)
#define WLSM_ALLOW 0
#define WLSM_DENY_ERRNO(err) (-(err))

#define WCG_ALLOW 1
#define WCG_DENY 0