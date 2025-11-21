//
// Created by usr on 26/10/2025.
//

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
	__type(value, struct WSocketRules);
	__uint(max_entries, 0xffff);
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

static __always_inline struct WSocketEvent* MakeSocketEvent(__u64 Cookie, __u8 EventType)
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

	SocketEvent->CgroupId = bpf_get_current_cgroup_id();
	SocketEvent->PidTgId = bpf_get_current_pid_tgid();
	SocketEvent->Cookie = Cookie;
	SocketEvent->EventType = EventType;

	return SocketEvent;
}

static __always_inline struct WSocketRules* GetSocketRules(__u64 Cookie)
{
	struct WSocketRules* Rules = bpf_map_lookup_elem(&socket_rules, &Cookie);
	return Rules;
}