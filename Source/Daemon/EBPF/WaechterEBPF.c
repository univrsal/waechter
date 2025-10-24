#include <linux/types.h>
#include <bpf/bpf_helpers.h>
#include <linux/bpf.h>

#ifndef PACKET_HEADER_SIZE
#define PACKET_HEADER_SIZE 128
#endif

#ifndef PACKET_RING_SIZE
// Default ring buffer capacity in bytes (1 MiB). Can be overridden via compile-time define.
#define PACKET_RING_SIZE (1 << 20)
#endif

struct packet_data
{
	__u8 raw_data[PACKET_HEADER_SIZE];
	__u64 cookie;
	__u64 pid_tgid;
	__u64 cgroup_id;
	__u64 bytes;
	__u64 ts; // kernel timestamp (ns) to help detect drops or ordering in userspace
	__u8 direction;
};

// Replace the array map (single element) with a ring buffer for events.
// The ring buffer stores raw packet_data entries for userspace to consume.
struct
{
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, PACKET_RING_SIZE);
} packet_ring SEC(".maps");

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
		pd->direction = 1; // egress
		pd->bytes = (__u64)skb->len;
		pd->ts = bpf_ktime_get_ns();

		// Bound copy length and avoid zero-sized read per verifier requirements
		__u32 slen = skb->len;
		__u32 len  = PACKET_HEADER_SIZE;
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
	(void)skb;
	return SK_PASS;
}

char LICENSE[] SEC("license") = "Proprietary";
