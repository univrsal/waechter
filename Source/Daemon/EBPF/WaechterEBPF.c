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

#include "EBPFCommon.h"

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

struct packet_data
{
	__u8  raw_data[PACKET_HEADER_SIZE];
	__u64 cookie;
	__u64 pid_tgid;
	__u64 cgroup_id;
	__u64 bytes;
	__u64 ts; // kernel timestamp (ns) to help detect drops or ordering in userspace
	__u8  direction;
};

struct socket_event
{
	__u8  event_type; // enum ENetEventType
	__u64 cookie;
	__u64 pid_tgid;
	__u64 cgroup_id;
};

// stores socket identity (PID/TGID + cgroup) per socket cookie
struct
{
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, PACKET_RING_SIZE);
} socket_event_ring SEC(".maps");

// stores raw packet_data entries for userspace to consume.
struct
{
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, PACKET_RING_SIZE);
} packet_ring SEC(".maps");

static __always_inline void push_socket_event(__u64 cookie, __u8 event_type)
{
	if (cookie == 0)
	{
		// No point in processing this as we want to map cookie <-> pid/tgid
		return;
	}

	struct socket_event* se = (struct socket_event*)bpf_ringbuf_reserve(&socket_event_ring, sizeof(struct socket_event), 0);
	if (!se)
	{
		bpf_printk("push_socket_event: reserve NULL cookie=%llu event=%u\n", cookie, event_type);
		return;
	}

	se->cgroup_id = bpf_get_current_cgroup_id();
	se->pid_tgid = bpf_get_current_pid_tgid();
	se->cookie = cookie;
	se->event_type = event_type;

	bpf_printk("push_socket_event: cookie=%llu event=%u pid_tgid=%llu cgroup=%llu\n",
		se->cookie, se->event_type, se->pid_tgid, se->cgroup_id);

	bpf_ringbuf_submit(se, 0);
}

// 1) Runs on socket() creation; tags the new socket with current PID/TGID.
SEC("cgroup/sock_create")
int on_sock_create(struct bpf_sock* sk)
{
	__u64 cookie = bpf_get_socket_cookie(sk);
	bpf_printk("on_sock_create: cookie=%llu\n", cookie);
	push_socket_event(cookie, NE_SocketCreate);
	return 1; // allow
}

// 2) Runs on connect(); tags client sockets with current PID/TGID.
SEC("cgroup/connect4")
int on_connect4(struct bpf_sock_addr* ctx)
{
	__u64 cookie = bpf_get_socket_cookie(ctx);
	bpf_printk("on_connect4: cookie=%llu\n", cookie);
	push_socket_event(cookie, NE_SocketConnect_4);
	return 1; // allow
}

SEC("cgroup/connect6")
int on_connect6(struct bpf_sock_addr* ctx)
{
	bpf_printk("on_connect6: entered\n");
	__u64 cookie = bpf_get_socket_cookie(ctx);
	bpf_printk("on_connect6: cookie=%llu\n", cookie);
	push_socket_event(cookie, NE_SocketConnect_6);
	return 1; // allow
}

// 3) Runs in the accept() caller’s context; tags accepted server sockets.
#ifdef HAVE_VMLINUX
SEC("lsm/socket_accept")
int BPF_PROG(socket_accept, struct socket* sock, struct socket* newsock)
{
	struct sock* sk = newsock->sk; // preserve trusted pointer type
	if (!sk)
		return 0;

	__u64 cookie = bpf_get_socket_cookie(sk);
	bpf_printk("socket_accept: cookie=%llu\n", cookie);
	push_socket_event(cookie, NE_SocketAccept);
	return 0; // allow
}
#else
	#error "LSM socket_accept hook requires CO-RE support with vmlinux.h"
#endif

SEC("xdp")
int xdp_waechter(struct xdp_md* ctx)
{
	(void)ctx;
	return XDP_PASS;
}

// cgroup_skb egress: capture PID and cgroup for socket cookie
SEC("cgroup_skb/egress")
int cgskb_egress(struct __sk_buff* skb)
{
	// Reserve space in the ring buffer for a packet_data entry
	struct packet_data* pd = (struct packet_data*)bpf_ringbuf_reserve(&packet_ring, sizeof(struct packet_data), 0);
	if (pd)
	{
		/* zero the entire destination buffer/struct */
		__builtin_memset(pd, 0, sizeof(*pd));

		pd->cookie = bpf_get_socket_cookie(skb);
		pd->pid_tgid = bpf_get_current_pid_tgid();
		pd->cgroup_id = bpf_get_current_cgroup_id();
		pd->direction = PD_Outgoing;
		pd->bytes = (__u64)skb->len;
		pd->ts = bpf_ktime_get_ns();

		// Bound copy length and avoid zero-sized read per verifier requirements
		__u32 slen = skb->len;
		__u32 len = PACKET_HEADER_SIZE;
		if (slen < PACKET_HEADER_SIZE)
			len = slen;
		if (len > 0)
			bpf_skb_load_bytes(skb, 0, pd->raw_data, len);

		// Submit the filled entry to the ring buffer. If the ring is full, reserve would have returned NULL.
		bpf_ringbuf_submit(pd, 0);
	}

	return SK_PASS;
}

// cgroup_skb ingress: account and optionally enforce for traffic delivered to the process
SEC("cgroup_skb/ingress")
int cgskb_ingress(struct __sk_buff* skb)
{
	// Reserve space in the ring buffer for a packet_data entry
	struct packet_data* pd = (struct packet_data*)bpf_ringbuf_reserve(&packet_ring, sizeof(struct packet_data), 0);
	if (pd)
	{
		/* zero the entire destination buffer/struct */
		__builtin_memset(pd, 0, sizeof(*pd));

		pd->cookie = bpf_get_socket_cookie(skb);
		pd->pid_tgid = bpf_get_current_pid_tgid();
		pd->cgroup_id = bpf_get_current_cgroup_id();
		pd->direction = PD_Incoming;
		pd->bytes = (__u64)skb->len;
		pd->ts = bpf_ktime_get_ns();

		// Bound copy length and avoid zero-sized read per verifier requirements
		__u32 slen = skb->len;
		__u32 len = PACKET_HEADER_SIZE;
		if (slen < PACKET_HEADER_SIZE)
			len = slen;
		if (len > 0)
			bpf_skb_load_bytes(skb, 0, pd->raw_data, len);

		// Submit the filled entry to the ring buffer. If the ring is full, reserve would have returned NULL.
		bpf_ringbuf_submit(pd, 0);
	}
	return SK_PASS;
}

char LICENSE[] SEC("license") = "Dual BSD/GPL";
