/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#if __EMSCRIPTEN__
	#include <cstdint>
#endif
#if defined(__linux__)
#ifndef EBPF_COMMON
	#include <linux/types.h>
#endif
#else
typedef uint8_t  __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef uint64_t __u64;
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
	NE_SocketBind_4,
	NE_SocketBind_6,
	NE_TCPSocketEstablished_4,
	NE_TCPSocketEstablished_6,
	NE_TCPSocketListening,
	NE_SocketAccept_4,
	NE_SocketAccept_6,
	NE_SocketClosed,
	NE_Traffic
};

enum ESwitchState : __u8
{
	SS_None = 0,
	SS_Block = 1,
	SS_Allow = 2
};

struct WTrafficItemRulesBase
{
	enum ESwitchState UploadSwitch;
	enum ESwitchState DownloadSwitch;
	// Used to route packets to the correct tc class for shaping
	__u32 UploadMark;
	__u32 DownloadMark;
};

struct WSocketBindEventData
{
	__u32 UserPort;
	__u8  bImplicitBind;
	union
	{
		__u32 Addr4;
		__u32 Addr6[4];
	};
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
	__u32 Family;
	__u32 Type;
};

struct WSocketAcceptEventData
{
	__u32 SourcePort;
	__u32 DestinationPort;
	__u32 Family;
	__u32 Type;
	union
	{
		__u32 SourceAddr4;
		__u32 SourceAddr6[4];
	};

	union
	{
		__u32 DestinationAddr4;
		__u32 DestinationAddr6[4];
	};
};

struct WSocketEventData
{
	union
	{
		struct WSocketConnectEventData        ConnectEventData;
		struct WSocketTrafficEventData        TrafficEventData;
		struct WSocketCreateEventData         SocketCreateEventData;
		struct WSocketTCPEstablishedEventData TCPSocketEstablishedEventData;
		struct WSocketBindEventData           SocketBindEventData;
		struct WSocketAcceptEventData         SocketAcceptEventData;
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
