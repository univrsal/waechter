/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: GPL-3.0-or-later
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

	// Populate port_to_pid when a socket enters LISTEN state.
	// This is essential on kernels without BPF LSM (e.g. Ubuntu) where
	// the lsm/socket_bind hook doesn't fire, leaving port_to_pid empty.
	// Without this, accepted connections can't be attributed to the correct process.
	__u16 Lport = ctx->sport;
	if (Lport != 0 && SharedData->Pid != 0)
	{
		bpf_map_update_elem(&port_to_pid, &Lport, &SharedData->Pid, BPF_ANY);
	}

	return 0;
}

SEC("fentry/tcp_set_state")
int BPF_PROG(on_tcp_set_state, struct sock* Sk, int Newstate)
{
	// fentry fires before the function body, so skc_state still holds the OLD state
	u8 Oldstate = BPF_CORE_READ(Sk, __sk_common.skc_state);

	if (Newstate == TCP_CLOSE)
	{
		struct WSocketEvent* Event = MakeSocketEvent(bpf_get_socket_cookie(Sk), NE_SocketClosed);

		if (Event)
		{
			bpf_ringbuf_submit(Event, 0);
		}

		// Only clean up port_to_pid mapping if the LISTENING socket itself is closing.
		// Accepted (child) connections share the same local port as the listener,
		// so we must NOT delete the mapping when an accepted connection closes.
		if (Oldstate == TCP_LISTEN)
		{
			u16 Lport = BPF_CORE_READ(Sk, __sk_common.skc_num);
			if (Lport != 0)
			{
				bpf_map_delete_elem(&port_to_pid, &Lport);
			}
		}
	}
	else if (Newstate == TCP_LISTEN)
	{
		// When a socket enters LISTEN state, populate port_to_pid.
		// This is essential on kernels without BPF LSM (e.g. Ubuntu) where
		// the lsm/socket_bind hook doesn't fire, leaving port_to_pid empty.
		u16 Lport = BPF_CORE_READ(Sk, __sk_common.skc_num);
		if (Lport != 0)
		{
			__u64 PidTgid = bpf_get_current_pid_tgid();
			__u32 Pid = (__u32)(PidTgid >> 32);
			if (Pid != 0)
			{
				bpf_map_update_elem(&port_to_pid, &Lport, &Pid, BPF_ANY);
			}
		}
	}
	else if (Newstate == TCP_ESTABLISHED)
	{
		// Local port: host-endian
		u16 Lport = BPF_CORE_READ(Sk, __sk_common.skc_num);

		// Determine if this is a server-side (accepted) connection.
		// For accepted connections tcp_set_state fires in softirq context,
		// so bpf_get_current_pid_tgid() returns an arbitrary PID.
		// Use port_to_pid (populated at bind time) to find the real owner.
		__u32* OwnerPid = NULL;
		if (Lport != 0)
		{
			OwnerPid = bpf_map_lookup_elem(&port_to_pid, &Lport);
		}

		__u64 Cookie = bpf_get_socket_cookie(Sk);
		if (!OwnerPid)
		{
			__u64 FallbackPid = bpf_get_current_pid_tgid();
		}

		// For client-side (outgoing) connections, populate port_to_pid
		// only if there isn't already an entry (don't overwrite server mappings).
		if (!OwnerPid && Lport != 0)
		{
			__u64 PidTgid = bpf_get_current_pid_tgid();
			__u32 Pid = (__u32)(PidTgid >> 32);
			bpf_map_update_elem(&port_to_pid, &Lport, &Pid, BPF_NOEXIST);
		}

		struct WSocketEvent* Event = MakeSocketEvent2(bpf_get_socket_cookie(Sk), NE_TCPSocketEstablished_4, false);

		if (Event)
		{
			// Override PidTgId: use the real owner PID from port_to_pid if available,
			// otherwise fall back to bpf_get_current_pid_tgid()
			if (OwnerPid && *OwnerPid != 0)
			{
				Event->PidTgId = ((__u64)(*OwnerPid)) << 32;
			}
			else
			{
				Event->PidTgId = bpf_get_current_pid_tgid();
			}
			Event->CgroupId = bpf_get_current_cgroup_id();

			// Family
			u16 Family = BPF_CORE_READ(Sk, __sk_common.skc_family);

			Event->Data.TCPSocketEstablishedEventData.UserPort = Lport;
			Event->Data.TCPSocketEstablishedEventData.bIsAccept = OwnerPid ? 1 : 0;

			// Remote port (network-endian in kernel, convert to host)
			u16 Dport = BPF_CORE_READ(Sk, __sk_common.skc_dport);
			Event->Data.TCPSocketEstablishedEventData.RemotePort = bpf_ntohs(Dport);

			if (Family == AF_INET)
			{
				// IPv4 local address
				__u32 Laddr4 = BPF_CORE_READ(Sk, __sk_common.skc_rcv_saddr);
				Event->Data.TCPSocketEstablishedEventData.Addr4 = Laddr4;
				// IPv4 remote address
				__u32 Daddr4 = BPF_CORE_READ(Sk, __sk_common.skc_daddr);
				Event->Data.TCPSocketEstablishedEventData.RemoteAddr4 = Daddr4;
				Event->EventType = NE_TCPSocketEstablished_4;
			}
			else if (Family == AF_INET6)
			{
				Event->EventType = NE_TCPSocketEstablished_6;
				// IPv6 local address
				BPF_CORE_READ_INTO(
					&Event->Data.TCPSocketEstablishedEventData.Addr6, Sk, __sk_common.skc_v6_rcv_saddr.in6_u.u6_addr32);
				// IPv6 remote address
				BPF_CORE_READ_INTO(&Event->Data.TCPSocketEstablishedEventData.RemoteAddr6, Sk,
					__sk_common.skc_v6_daddr.in6_u.u6_addr32);
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
