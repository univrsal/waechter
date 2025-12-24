/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include "EBPFInternal.h"

struct
{
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, __u64);   // pid_tgid
	__type(value, __u64); // socket cookie
	__uint(max_entries, 16384);
} UdpSocketCookieByPid SEC(".maps");

struct
{
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, __u64);   // pid_tgid
	__type(value, __u64); // sock ptr
	__uint(max_entries, 16384);
} UdpSkByPid SEC(".maps");

SEC("fexit/udp_sendmsg")
int BPF_PROG(fexit_udp_sendmsg, struct sock* Sk, struct msghdr* msg, size_t len)
{
	if (!Sk)
	{
		return 0;
	}

	__u64 Pid = bpf_get_current_pid_tgid();
	__u64 Cookie = bpf_get_socket_cookie(Sk);

	bpf_map_update_elem(&UdpSocketCookieByPid, &Pid, &Cookie, BPF_ANY);
	return 0;
}

// Helper to emit event after sendmsg returns; shared by v4/v6 hooks
static __always_inline int EmitIfBound(struct sock* Sk, __u64 Cookie)
{
	if (!Sk)
	{
		return 0;
	}

	__u16 Family = BPF_CORE_READ(Sk, __sk_common.skc_family);
	__u16 SPortBe = BPF_CORE_READ((struct inet_sock*)Sk, inet_sport);
	__u16 SPort = bpf_ntohs(SPortBe);

	if (SPort == 0)
	{
		return 0; // still not bound
	}

	__u8 EventType = 0;
	switch (Family)
	{
		case AF_INET:
			EventType = NE_SocketBind_4;
			break;
		case AF_INET6:
			EventType = NE_SocketBind_6;
			break;
		default:
			return 0;
	}

	struct WSocketEvent* Event = MakeSocketEvent(Cookie, EventType);

	if (!Event)
	{
		return 0;
	}

	Event->Data.SocketBindEventData.UserPort = SPort;
	Event->Data.SocketBindEventData.bImplicitBind = 1;

	if (Family == AF_INET)
	{
		// Note: for unconnected sockets, inet_saddr can remain 0 (INADDR_ANY)
		__u32 SAddr = BPF_CORE_READ((struct inet_sock*)Sk, inet_saddr);
		if (SAddr == 0)
		{
			// Set it to localhost
			SAddr = __bpf_htonl(0x7F000001);
		}
		Event->Data.SocketBindEventData.Addr4 = SAddr;
	}
	else if (Family == AF_INET6)
	{
		__builtin_memset(&Event->Data.SocketBindEventData.Addr6, 0, sizeof(Event->Data.SocketBindEventData.Addr6));
		// For IPv6, local address (if any) is in pinet6->saddr
		struct ipv6_pinfo* P6 = BPF_CORE_READ((struct inet_sock*)Sk, pinet6);
		if (P6)
		{
			struct in6_addr SAddr6 = BPF_CORE_READ(P6, saddr);
			__builtin_memcpy(&Event->Data.SocketBindEventData.Addr6, &SAddr6.in6_u.u6_addr8, 16);
		}
	}
	bpf_ringbuf_submit(Event, 0);

	return 0;
}

// Pair entry/return hooks to get the sk reliably per task
SEC("kprobe/udp_sendmsg")
int BPF_KPROBE(enter_udp4_sendmsg, struct sock* Sk, struct msghdr* Msg, size_t Len)
{
	__u64 Pid = bpf_get_current_pid_tgid();
	__u64 Psk = (__u64)Sk;
	bpf_map_update_elem(&UdpSkByPid, &Pid, &Psk, BPF_ANY);
	return 0;
}

SEC("kretprobe/udp_sendmsg")
int BPF_KRETPROBE(ret_udp4_sendmsg)
{
	int    Result = 0;
	__u64  Pid = bpf_get_current_pid_tgid();
	__u64* Psk = bpf_map_lookup_elem(&UdpSkByPid, &Pid);
	__u64* Cookie = bpf_map_lookup_elem(&UdpSocketCookieByPid, &Pid);

	if (!Psk)
	{
		return 0;
	}

	if (!Cookie)
	{
		goto Cleanup;
	}

	struct sock* Sk = (struct sock*)(*Psk);
	bpf_map_delete_elem(&UdpSkByPid, &Pid);
	Result = EmitIfBound(Sk, *Cookie);
Cleanup:
	if (Psk)
	{
		bpf_map_delete_elem(&UdpSkByPid, &Pid);
	}

	if (Cookie)
	{
		bpf_map_delete_elem(&UdpSocketCookieByPid, &Pid);
	}
	return Result;
}

SEC("kprobe/udpv6_sendmsg")
int BPF_KPROBE(enter_udp6_sendmsg, struct sock* Sk, struct msghdr* Msg, size_t Len)
{
	__u64 Pid = bpf_get_current_pid_tgid();
	__u64 Psk = (__u64)Sk;
	bpf_map_update_elem(&UdpSkByPid, &Pid, &Psk, BPF_ANY);
	return 0;
}

SEC("kretprobe/udpv6_sendmsg")
int BPF_KRETPROBE(ret_udp6_sendmsg)
{
	int   Result = 0;
	__u64 Pid = bpf_get_current_pid_tgid();

	__u64* Cookie = bpf_map_lookup_elem(&UdpSocketCookieByPid, &Pid);
	__u64* Psk = bpf_map_lookup_elem(&UdpSkByPid, &Pid);

	if (!Psk)
	{
		goto Cleanup;
	}

	if (!Cookie)
	{
		goto Cleanup;
	}
	struct sock* Sk = (struct sock*)(*Psk);
	bpf_map_delete_elem(&UdpSkByPid, &Pid);
	Result = EmitIfBound(Sk, *Cookie);
Cleanup:
	if (Psk)
	{
		bpf_map_delete_elem(&UdpSkByPid, &Pid);
	}

	if (Cookie)
	{
		bpf_map_delete_elem(&UdpSocketCookieByPid, &Pid);
	}
	return Result;
}