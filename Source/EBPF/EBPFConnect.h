/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

// Tracks sockets connecting
#pragma once
#include "EBPFInternal.h"

SEC("tracepoint/sock/inet_sock_set_state")
int tracepoint__inet_sock_set_state(struct trace_event_raw_inet_sock_set_state* ctx)
{
	if (ctx->protocol != IPPROTO_TCP)
	{
		return 0;
	}
	// Do we care about other states?
	if (ctx->newstate != TCP_LISTEN)
	{
		return 0;
	}
	__u64 Key = (__u64)ctx->skaddr;

	struct WSharedSocketData* SharedData = bpf_map_lookup_elem(&shared_socket_data_map, &Key);

	if (!SharedData)
	{
		return 0;
	}

	struct WSocketEvent* Event = MakeSocketEvent(SharedData->Cookie, NE_TCPSocketListening);
	if (Event)
	{
		bpf_ringbuf_submit(Event, 0);
	}
	return 0;
}

SEC("fentry/tcp_set_state")
int BPF_PROG(on_tcp_set_state, struct sock* Sk, int Newstate)
{
	if (Newstate == TCP_CLOSE)
	{
		struct WSocketEvent* Event = MakeSocketEvent(bpf_get_socket_cookie(Sk), NE_SocketClosed);

		if (Event)
		{
			bpf_ringbuf_submit(Event, 0);
		}
	}
	else if (Newstate == TCP_ESTABLISHED)
	{
		struct WSocketEvent* Event = MakeSocketEvent(bpf_get_socket_cookie(Sk), NE_TCPSocketEstablished_4);
		// Family
		u16 Family = BPF_CORE_READ(Sk, __sk_common.skc_family);

		// Local port: host-endian
		u16 Lport = BPF_CORE_READ(Sk, __sk_common.skc_num);

		if (Event)
		{
			Event->Data.TCPSocketEstablishedEventData.UserPort = Lport;

			if (Family == AF_INET)
			{
				// IPv4 address
				__u32 Laddr4 = BPF_CORE_READ(Sk, __sk_common.skc_rcv_saddr);
				Event->Data.TCPSocketEstablishedEventData.Addr4 = Laddr4;
				Event->EventType = NE_TCPSocketEstablished_4;
			}
			else if (Family == AF_INET6)
			{
				Event->EventType = NE_TCPSocketEstablished_6;
				// IPv6 address
				BPF_CORE_READ_INTO(
					&Event->Data.TCPSocketEstablishedEventData.Addr6, Sk, __sk_common.skc_v6_rcv_saddr.in6_u.u6_addr32);
			}
			bpf_ringbuf_submit(Event, 0);
		}
	}
	return 0;
}

SEC("cgroup/connect4")
int on_connect4(struct bpf_sock_addr* Ctx)
{
	__u64 Cookie = bpf_get_socket_cookie(Ctx);
	__u64 PidTgid = bpf_get_current_pid_tgid();
	__u32 Tgid = (__u32)(PidTgid >> 32);
	if (Tgid == 0)
	{
		return WCG_ALLOW;
	}

	struct WSocketEvent* SocketEvent = MakeSocketEvent(Cookie, NE_SocketConnect_4);

	if (SocketEvent)
	{
		SocketEvent->Data.ConnectEventData.UserFamily = Ctx->user_family;
		SocketEvent->Data.ConnectEventData.Protocol = Ctx->protocol;
		SocketEvent->Data.ConnectEventData.UserPort = bpf_ntohs(Ctx->user_port);

		SocketEvent->Data.ConnectEventData.Addr4 = Ctx->user_ip4;
		bpf_ringbuf_submit(SocketEvent, 0);
	}
	return WCG_ALLOW;
}

SEC("cgroup/connect6")
int on_connect6(struct bpf_sock_addr* Ctx)
{
	__u64 Cookie = bpf_get_socket_cookie(Ctx);
	__u64 PidTgid = bpf_get_current_pid_tgid();
	__u32 Tgid = (__u32)(PidTgid >> 32);
	if (Tgid == 0)
	{
		return WCG_ALLOW;
	}

	struct WSocketEvent* SocketEvent = MakeSocketEvent(Cookie, NE_SocketConnect_6);
	if (SocketEvent)
	{
		SocketEvent->Data.ConnectEventData.UserFamily = Ctx->user_family;
		SocketEvent->Data.ConnectEventData.Protocol = Ctx->protocol;
		SocketEvent->Data.ConnectEventData.UserPort = bpf_ntohs(Ctx->user_port);
		SocketEvent->Data.ConnectEventData.Addr6[0] = Ctx->user_ip6[0];
		SocketEvent->Data.ConnectEventData.Addr6[1] = Ctx->user_ip6[1];
		SocketEvent->Data.ConnectEventData.Addr6[2] = Ctx->user_ip6[2];
		SocketEvent->Data.ConnectEventData.Addr6[3] = Ctx->user_ip6[3];

		bpf_ringbuf_submit(SocketEvent, 0);
	}
	return WCG_ALLOW;
}
