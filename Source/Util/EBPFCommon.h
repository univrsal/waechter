//
// Created by usr on 25/10/2025.
//

#pragma once

#define PACKET_HEADER_SIZE 128

#define WUNUSED(x) (void)(x)
#define WLSM_ALLOW 0
#define WLSM_DENY_ERRNO(err) (-(err))

#define WCG_ALLOW 1
#define WCG_DENY 0

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

struct WSocketEvent
{
	__u8  EventType; // enum ENetEventType
	__u64 Cookie;
	__u64 PidTgId;
	__u64 CgroupId;
};

struct WPacketData
{
	__u8  RawData[PACKET_HEADER_SIZE];
	__u64 Cookie;
	__u64 PidTgId;
	__u64 CgroupId;
	__u64 Bytes;
	__u64 Timestamp; // kernel timestamp (ns)
	__u8  Direction;
};