#include <bpf/bpf_helpers.h>
#include <linux/bpf.h>

SEC("xdp")
int xdp_waechter(struct xdp_md* ctx)
{
	return XDP_PASS;
}

// cgroup_skb egress: capture PID and cgroup for socket cookie
SEC("cgroup_skb/egress")
int cgskb_egress(struct __sk_buff* skb)
{
	return 1;
}

// cgroup_skb ingress: account and optionally enforce for traffic delivered to the process
SEC("cgroup_skb/ingress")
int cgskb_ingress(struct __sk_buff* skb)
{
	return 1; // continue
}

char LICENSE[] SEC("license") = "Proprietary";
