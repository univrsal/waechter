#include <linux/types.h>
#include <bpf/bpf_helpers.h>
#include <linux/bpf.h>

#define CAPTURE_LEN 256
struct packet_data
{
	__u8 raw_data[256];
};

// Global stats (index 0)
struct
{
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 1);
	__type(key, __u32);
	__type(value, struct packet_data);
} packet_stats SEC(".maps");

SEC("xdp")
int xdp_waechter(struct xdp_md* ctx)
{
	return XDP_PASS;
}

// cgroup_skb egress: capture PID and cgroup for socket cookie
SEC("cgroup_skb/egress")
int cgskb_egress(struct __sk_buff* skb)
{
	// Copy first CAPTURE_LEN bytes of the packet to the map
	__u32              gk = 0;
	struct packet_data* pd = (struct packet_data*)bpf_map_lookup_elem(&packet_stats, &gk);
	if (pd)
	{
		/* zero the destination buffer (CAPTURE_LEN is a compile-time constant) */
		__builtin_memset(pd->raw_data, 0, CAPTURE_LEN);

		// Bound copy length and avoid zero-sized read per verifier requirements
		__u32 slen = skb->len;
		__u32 len  = CAPTURE_LEN;
		if (slen < CAPTURE_LEN)
			len = slen;
		if (len > 0)
			bpf_skb_load_bytes(skb, 0, pd->raw_data, len);
	}

	return SK_PASS;
}

// cgroup_skb ingress: account and optionally enforce for traffic delivered to the process
SEC("cgroup_skb/ingress")
int cgskb_ingress(struct __sk_buff* skb)
{
	return SK_PASS;
}

char LICENSE[] SEC("license") = "Proprietary";
