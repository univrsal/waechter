/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include "EBPFInternal.h"

#ifdef HAVE_VMLINUX
SEC("lsm/sock_graft")
int BPF_PROG(sock_graft, struct sock* Sk, struct socket* Parent)
{
	if (!Sk)
	{
		return WLSM_ALLOW;
	}

	// Only care about IP socket families
	__u16 Family = Sk->__sk_common.skc_family;
	if (Family != AF_INET && Family != AF_INET6)
	{
		return WLSM_ALLOW;
	}

	__u64 Cookie = bpf_get_socket_cookie(Sk);

	// sock_graft fires in softirq context during TCP handshake completion,
	// so bpf_get_current_pid_tgid() returns whatever process was interrupted
	// by the softirq — NOT the process that called accept(). We must look up
	// the actual owning PID from the local port via port_to_pid map, which
	// was populated when the listening socket was bound.
	__u16  Lport = Sk->__sk_common.skc_num; // host-endian
	__u32* OwnerPid = bpf_map_lookup_elem(&port_to_pid, &Lport);

	struct WSocketEvent* Event = MakeSocketEvent2(Cookie, NE_SocketAccept_4, false);
	if (!Event)
	{
		return WLSM_ALLOW;
	}

	// Override PidTgId with the actual owner PID from port_to_pid lookup
	if (OwnerPid && *OwnerPid != 0)
	{
		Event->PidTgId = ((__u64)(*OwnerPid)) << 32;
	}
	else
	{
		// Fallback: use bpf_get_current_pid_tgid() even though it may be wrong
		// in softirq context. Better than nothing.
		Event->PidTgId = bpf_get_current_pid_tgid();
	}
	Event->CgroupId = bpf_get_current_cgroup_id();

	Event->Data.SocketAcceptEventData.Family = Family;
	Event->Data.SocketAcceptEventData.Type = Parent ? Parent->type : 0;

	if (Family == AF_INET)
	{
		Event->Data.SocketAcceptEventData.SourceAddr4 = Sk->__sk_common.skc_rcv_saddr;
		Event->Data.SocketAcceptEventData.DestinationAddr4 = Sk->__sk_common.skc_daddr;
		Event->Data.SocketAcceptEventData.SourcePort = Lport; // already host-endian
		Event->Data.SocketAcceptEventData.DestinationPort = bpf_ntohs(Sk->__sk_common.skc_dport);
	}
	else if (Family == AF_INET6)
	{
		Event->EventType = NE_SocketAccept_6;
		BPF_CORE_READ_INTO(
			&Event->Data.SocketAcceptEventData.SourceAddr6, Sk, __sk_common.skc_v6_rcv_saddr.in6_u.u6_addr32);
		BPF_CORE_READ_INTO(
			&Event->Data.SocketAcceptEventData.DestinationAddr6, Sk, __sk_common.skc_v6_daddr.in6_u.u6_addr32);
		Event->Data.SocketAcceptEventData.SourcePort = Lport; // already host-endian
		Event->Data.SocketAcceptEventData.DestinationPort = bpf_ntohs(Sk->__sk_common.skc_dport);
	}

	bpf_ringbuf_submit(Event, 0);
	return WLSM_ALLOW;
}

#else
	#error "LSM socket_accept hook requires CO-RE support with vmlinux.h"
#endif
