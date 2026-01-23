/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include "EBPFInternal.h"

SEC("lsm/socket_bind")
int BPF_PROG(socket_bind, struct socket* Sock, struct sockaddr* Addr, int Addrlen)
{

	struct sock* Socket = Sock->sk; // preserve trusted pointer type
	if (!Socket)
	{
		return WLSM_ALLOW;
	}

	if (Sock->type != SOCK_STREAM && Sock->type != SOCK_DGRAM)
	{
		return WLSM_ALLOW;
	}

	if (Addr && Addr->sa_family != AF_INET && Addr->sa_family != AF_INET6)
	{
		return WLSM_ALLOW;
	}

	struct sockaddr_in  Sa = {};
	struct sockaddr_in6 Sa6 = {};

	if (Addr->sa_family == AF_INET)
	{
		if (bpf_probe_read_kernel(&Sa, sizeof(Sa), Addr))
			return 0;
	}
	else if (Addr->sa_family == AF_INET6)
	{
		if (bpf_probe_read_kernel(&Sa6, sizeof(Sa6), Addr))
			return 0;
	}

	struct WSocketEvent* SocketEvent = MakeSocketEvent(bpf_get_socket_cookie(Socket), NE_SocketBind_4);

	if (SocketEvent)
	{
		__u16 Port = 0;
		if (Addr->sa_family == AF_INET)
		{
			Port = bpf_ntohs(Sa.sin_port);
			SocketEvent->Data.SocketBindEventData.UserPort = Port;
			SocketEvent->Data.SocketBindEventData.Addr4 = Sa.sin_addr.s_addr;
		}
		else if (Addr->sa_family == AF_INET6)
		{
			SocketEvent->EventType = NE_SocketBind_6;
			Port = bpf_ntohs(Sa6.sin6_port);
			SocketEvent->Data.SocketBindEventData.UserPort = Port;
			SocketEvent->Data.SocketBindEventData.Addr6[0] = Sa6.sin6_addr.in6_u.u6_addr32[0];
			SocketEvent->Data.SocketBindEventData.Addr6[1] = Sa6.sin6_addr.in6_u.u6_addr32[1];
			SocketEvent->Data.SocketBindEventData.Addr6[2] = Sa6.sin6_addr.in6_u.u6_addr32[2];
			SocketEvent->Data.SocketBindEventData.Addr6[3] = Sa6.sin6_addr.in6_u.u6_addr32[3];
		}
		SocketEvent->Data.SocketBindEventData.bImplicitBind = 0;

		// Populate port_to_pid map for PID-based rate limiting fallback
		if (Port != 0)
		{
			__u64 PidTgid = bpf_get_current_pid_tgid();
			__u32 Pid = (__u32)(PidTgid >> 32);
			bpf_map_update_elem(&port_to_pid, &Port, &Pid, BPF_ANY);
		}

		bpf_ringbuf_submit(SocketEvent, 0);
	}

	return 0;
}