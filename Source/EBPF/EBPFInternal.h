/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

// If CO-RE/BTF is available, prefer vmlinux.h over UAPI headers to avoid redefinitions.
#if __has_include("vmlinux.h")
	#include "vmlinux.h"
	#include <bpf/bpf_helpers.h>
	#include <bpf/bpf_tracing.h>
	#include <bpf/bpf_core_read.h>
	#include <bpf/bpf_endian.h>
	#define HAVE_VMLINUX 1
#else
	#include <linux/types.h>
	#include <linux/bpf.h>
	#include <bpf/bpf_helpers.h>
	#include <bpf/bpf_endian.h>
#endif

#ifndef PACKET_RING_SIZE
	// Default ring buffer capacity in bytes (1 MiB). Can be overridden via compile-time define.
	#define PACKET_RING_SIZE (1 << 20)
#endif

// Fallbacks for return codes when linux/bpf.h is not included
#ifndef SK_PASS
	#define SK_PASS 1
#endif
#ifndef XDP_PASS
	#define XDP_PASS 2
#endif

#define EBPF_COMMON
#include "EBPFCommon.h"

#define AF_INET 2
#define AF_INET6 10

// EtherType values for IPv4/IPv6 when checking __sk_buff->protocol
#ifndef ETH_P_IP
	#define ETH_P_IP 0x0800
#endif
#ifndef ETH_P_IPV6
	#define ETH_P_IPV6 0x86DD
#endif

struct
{
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, __u64); // Socket cookie
	__type(value, struct WTrafficItemRulesBase);
	__uint(max_entries, 0xffff);
	__uint(pinning, LIBBPF_PIN_BY_NAME); // We need to pin the map so it's both accessible in tcx and cgroup programs
} socket_rules SEC(".maps");

struct
{
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, PACKET_RING_SIZE);
} socket_event_ring SEC(".maps");

struct WSharedSocketData
{
	__u32 Pid;
	__u32 Family;
	__u64 Cookie;
};

struct
{
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 65536);
	__type(key, __u64);
	__type(value, struct WSharedSocketData);
} shared_socket_data_map SEC(".maps");

struct
{
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 65536);
	__type(key, __u16);
	__type(value, __u16);
} ingress_port_marks SEC(".maps");

// PID-based download marks for immediate application of rules to new sockets
// This serves as a fallback when port-based lookup fails (e.g., for newly opened ports)
struct
{
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 65536);
	__type(key, __u32);   // PID
	__type(value, __u32); // Download mark
} pid_download_marks SEC(".maps");

// Maps local port → PID, populated at TCP established time
// Used for looking up PID from ingress traffic on IFB device
struct
{
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 65536);
	__type(key, __u16);   // Local port
	__type(value, __u32); // PID
} port_to_pid SEC(".maps");

static __always_inline struct WSocketEvent* MakeSocketEvent2(__u64 Cookie, __u8 EventType, bool bWithPID)
{
	if (Cookie == 0)
	{
		// No point in processing this as we want to map cookie <-> pid/tgid
		return NULL;
	}

	struct WSocketEvent* SocketEvent =
		(struct WSocketEvent*)bpf_ringbuf_reserve(&socket_event_ring, sizeof(struct WSocketEvent), 0);
	if (!SocketEvent)
	{
		return NULL;
	}
	__builtin_memset(&SocketEvent->Data, 0, sizeof(struct WSocketEventData));

	if (bWithPID)
	{
		SocketEvent->CgroupId = bpf_get_current_cgroup_id();
		SocketEvent->PidTgId = bpf_get_current_pid_tgid();
	}
	else
	{
		SocketEvent->CgroupId = 0;
		SocketEvent->PidTgId = 0;
	}
	SocketEvent->Cookie = Cookie;
	SocketEvent->EventType = EventType;

	return SocketEvent;
}

static __always_inline struct WSocketEvent* MakeSocketEvent(__u64 Cookie, __u8 EventType)
{
	return MakeSocketEvent2(Cookie, EventType, true);
}

static __always_inline struct WTrafficItemRulesBase* GetSocketRules(__u64 Cookie)
{
	struct WTrafficItemRulesBase* Rules = bpf_map_lookup_elem(&socket_rules, &Cookie);
	return Rules;
}