//
// Created by usr on 08/11/2025.
//

#pragma once
#include "EBPFInternal.h"

#ifdef HAVE_VMLINUX
SEC("lsm/sock_graft")
int BPF_PROG(sock_graft, struct sock* Sk, struct socket* Parent)
{
	if (!Sk)
		return WLSM_ALLOW;

	__u64                Socket = bpf_get_socket_cookie(Sk);
	struct WSocketEvent* Event = MakeSocketEvent(Socket, NE_SocketAccept_4);
	if (!Event)
		return WLSM_ALLOW;

	__u16 const Family = Sk->__sk_common.skc_family;
	Event->Data.SocketAcceptEventData.Family = Family;
	Event->Data.SocketAcceptEventData.Type = Parent ? Parent->type : 0;

	if (Family == AF_INET)
	{
		Event->Data.SocketAcceptEventData.SourceAddr4 = Sk->__sk_common.skc_rcv_saddr;
		Event->Data.SocketAcceptEventData.DestinationAddr4 = Sk->__sk_common.skc_daddr;
		Event->Data.SocketAcceptEventData.SourcePort = bpf_ntohs(Sk->__sk_common.skc_num);
		Event->Data.SocketAcceptEventData.DestinationPort = bpf_ntohs(Sk->__sk_common.skc_dport);
	}
	else if (Family == AF_INET6)
	{
		Event->EventType = NE_SocketAccept_6;
		BPF_CORE_READ_INTO(
			&Event->Data.SocketAcceptEventData.SourceAddr6, Sk, __sk_common.skc_v6_rcv_saddr.in6_u.u6_addr32);
		BPF_CORE_READ_INTO(
			&Event->Data.SocketAcceptEventData.DestinationAddr6, Sk, __sk_common.skc_v6_daddr.in6_u.u6_addr32);
		Event->Data.SocketAcceptEventData.SourcePort = bpf_ntohs(Sk->__sk_common.skc_num);
		Event->Data.SocketAcceptEventData.DestinationPort = bpf_ntohs(Sk->__sk_common.skc_dport);
	}

	bpf_ringbuf_submit(Event, 0);
	return WLSM_ALLOW;
}

#else
	#error "LSM socket_accept hook requires CO-RE support with vmlinux.h"
#endif
