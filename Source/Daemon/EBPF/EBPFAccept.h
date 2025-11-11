//
// Created by usr on 08/11/2025.
//

#pragma once
#include "EBPFInternal.h"

#ifdef HAVE_VMLINUX
SEC("lsm/sock_graft")
int BPF_PROG(sock_graft, struct sock* sk, struct socket* parent)
{
	if (!sk)
		return WLSM_ALLOW;

	__u64                cookie = bpf_get_socket_cookie(sk);
	struct WSocketEvent* evt = MakeSocketEvent(cookie, NE_SocketAccept_4);
	if (!evt)
		return WLSM_ALLOW;

	__u16 const family = sk->__sk_common.skc_family;
	evt->Data.SocketAcceptEventData.Family = family;
	evt->Data.SocketAcceptEventData.Type = parent ? parent->type : 0;

	if (family == AF_INET)
	{
		evt->Data.SocketAcceptEventData.SourceAddr4 = sk->__sk_common.skc_rcv_saddr;
		evt->Data.SocketAcceptEventData.DestinationAddr4 = sk->__sk_common.skc_daddr;
		evt->Data.SocketAcceptEventData.SourcePort = bpf_ntohs(sk->__sk_common.skc_num);
		evt->Data.SocketAcceptEventData.DestinationPort = bpf_ntohs(sk->__sk_common.skc_dport);
	}
	else if (family == AF_INET6)
	{
		evt->EventType = NE_SocketAccept_6;
		BPF_CORE_READ_INTO(
			&evt->Data.SocketAcceptEventData.SourceAddr6, sk, __sk_common.skc_v6_rcv_saddr.in6_u.u6_addr32);
		BPF_CORE_READ_INTO(
			&evt->Data.SocketAcceptEventData.DestinationAddr6, sk, __sk_common.skc_v6_daddr.in6_u.u6_addr32);
		evt->Data.SocketAcceptEventData.SourcePort = bpf_ntohs(sk->__sk_common.skc_num);
		evt->Data.SocketAcceptEventData.DestinationPort = bpf_ntohs(sk->__sk_common.skc_dport);
	}

	bpf_ringbuf_submit(evt, 0);
	return WLSM_ALLOW;
}

#else
	#error "LSM socket_accept hook requires CO-RE support with vmlinux.h"
#endif
