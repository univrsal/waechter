//
// Created by usr on 25/10/2025.
//

#pragma once
#ifndef EBPF_COMMON
	#include <linux/types.h>
#endif

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
	NE_SocketConnect_6, // At this point we do not have the local address/port yet
	NE_TCPSocketEstablished_4,
	NE_TCPSocketEstablished_6,
	NE_TCPSocketListening,
	NE_SocketAccept,
	NE_SocketClosed,
	NE_SocketSendmsg_4,
	NE_SocketSendmsg_6,
	NE_Traffic
};

struct WSocketConnectEventData
{
	__u32 UserFamily;
	__u32 Protocol;
	__u32 UserPort;
	union
	{
		__u32 Addr4;
		__u32 Addr6[4];
	};
};

struct WSocketTCPEstablishedEventData
{
	__u32 UserPort;
	union
	{
		__u32 Addr4;
		__u32 Addr6[4];
	};
};

struct WSocketTrafficEventData
{
	__u8  RawData[PACKET_HEADER_SIZE];
	__u64 Bytes;
	__u64 Timestamp; // kernel timestamp (ns)
	__u8  Direction;
};

struct WSocketCreateEventData
{
	__u32 Protocol;
};

struct WSocketEventData
{
	union
	{
		struct WSocketConnectEventData        ConnectEventData;
		struct WSocketTrafficEventData        TrafficEventData;
		struct WSocketCreateEventData         SocketCreateEventData;
		struct WSocketTCPEstablishedEventData TCPSocketEstablishedEventData;
	};
};

struct WSocketEvent
{
	__u8  EventType; // enum ENetEventType
	__u64 Cookie;
	__u64 PidTgId;
	__u64 CgroupId;

	struct WSocketEventData Data;
};