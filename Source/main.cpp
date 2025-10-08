// Minimal libbpf-based loader: loads ebpf/waechter_bpf.o, attaches XDP and cgroup egress,
// and periodically prints stats from maps. Requires libbpf (pkg: libbpf-dev) and root.

#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <net/if.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/if_link.h>
#include <fcntl.h>
#include <signal.h>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#ifndef BPF_OBJECT_PATH
	#define BPF_OBJECT_PATH "ebpf/waechter_bpf.o"
#endif

static volatile bool stop = false;
static void          on_sigint(int)
{
	stop = true;
}

static void bump_memlock_rlimit()
{
	rlimit r = { RLIM_INFINITY, RLIM_INFINITY };
	setrlimit(RLIMIT_MEMLOCK, &r);
}

struct flow_stats
{
	unsigned long long bytes;
	unsigned long long packets;
};
struct proc_meta
{
	unsigned long long pid_tgid;
	unsigned long long cgroup_id;
	char               comm[16];
};

struct MapFds
{
	int global_stats = -1;
	int pid_stats = -1;
	int cgroup_stats = -1;
	int conn_stats = -1;
	int cookie_proc_map = -1;
};

static void print_stats(const MapFds& mfds)
{
	flow_stats fs{};
	uint32_t   gk = 0;
	if (mfds.global_stats >= 0 && bpf_map_lookup_elem(mfds.global_stats, &gk, &fs) == 0)
	{
		std::cout << "Global: " << fs.bytes << " bytes, " << fs.packets << " pkts\n";
	}

	// Print top few PIDs (simple linear scan with get_next_key)
	if (mfds.pid_stats >= 0)
	{
		std::cout << "Per-PID (non-zero):\n";
		uint32_t key = 0;
		int      ret = bpf_map_get_next_key(mfds.pid_stats, nullptr, &key);
		while (ret == 0)
		{
			flow_stats s{};
			if (bpf_map_lookup_elem(mfds.pid_stats, &key, &s) == 0 && (s.bytes || s.packets))
			{
				std::cout << "  PID " << key << ": " << s.bytes << "B, " << s.packets << " pkts\n";
			}
			uint32_t prev = key;
			ret = bpf_map_get_next_key(mfds.pid_stats, &prev, &key);
		}
	}

	// Print a few connections (cookie -> stats, with optional PID/comm from cookie_proc_map)
	if (mfds.conn_stats >= 0)
	{
		std::cout << "Per-connection (cookie):\n";
		uint64_t key = 0;
		int      printed = 0;
		int      ret = bpf_map_get_next_key(mfds.conn_stats, nullptr, &key);
		while (ret == 0 && printed < 20)
		{
			flow_stats s{};
			if (bpf_map_lookup_elem(mfds.conn_stats, &key, &s) == 0 && (s.bytes || s.packets))
			{
				proc_meta pm{};
				bool      have_meta = (mfds.cookie_proc_map >= 0 && bpf_map_lookup_elem(mfds.cookie_proc_map, &key, &pm) == 0);
				if (have_meta)
				{
					uint32_t pid = static_cast<uint32_t>(pm.pid_tgid >> 32);
					std::cout << "  cookie=" << key << " pid=" << pid << " comm=" << pm.comm
							  << ": " << s.bytes << "B, " << s.packets << " pkts\n";
				}
				else
				{
					std::cout << "  cookie=" << key << ": " << s.bytes << "B, " << s.packets << " pkts\n";
				}
				++printed;
			}
			uint64_t prev = key;
			ret = bpf_map_get_next_key(mfds.conn_stats, &prev, &key);
		}
	}
}

int main(int argc, char** argv)
{
	const char* iface = nullptr;
	const char* cgroup_path = nullptr;
	if (argc >= 2)
		iface = argv[1];
	if (argc >= 3)
	{
		if (std::strcmp(argv[2], "none") == 0)
			cgroup_path = nullptr; // explicitly disable cgroup attach
		else
			cgroup_path = argv[2];
	}
	else
	{
		cgroup_path = "/sys/fs/cgroup"; // default to cgroup v2 root (covers all processes)
	}
	if (!iface)
	{
		std::cerr << "Usage: " << argv[0] << " <iface> [cgroup_path|none] (default cgroup_path=/sys/fs/cgroup)" << std::endl;
		return 1;
	}

	bump_memlock_rlimit();
	signal(SIGINT, on_sigint);
	signal(SIGTERM, on_sigint);

	libbpf_set_strict_mode(LIBBPF_STRICT_ALL);

	int ifindex = if_nametoindex(iface);
	if (!ifindex)
	{
		perror("if_nametoindex");
		return 1;
	}

	bpf_object* obj = nullptr;
	obj = bpf_object__open_file(BPF_OBJECT_PATH, nullptr);
	if (!obj)
	{
		std::cerr << "Failed to open BPF object: " << BPF_OBJECT_PATH << std::endl;
		return 1;
	}
	if (bpf_object__load(obj) < 0)
	{
		std::cerr << "Failed to load BPF object" << std::endl;
		return 1;
	}

	// Find programs
	bpf_program* xdp_prog = bpf_object__find_program_by_name(obj, "xdp_waechter");
	bpf_program* cg_egress = bpf_object__find_program_by_name(obj, "cgskb_egress");
	bpf_program* cg_ingress = bpf_object__find_program_by_name(obj, "cgskb_ingress");
	if (!xdp_prog)
	{
		std::cerr << "xdp_waechter not found" << std::endl;
		return 1;
	}

	// BPF_PROG_TYPE_SOCKET_FILTER

	// Attach XDP (generic by default)
	int xdp_flags = XDP_FLAGS_UPDATE_IF_NOEXIST | XDP_FLAGS_SKB_MODE;
	int xdp_fd = bpf_program__fd(xdp_prog);
	if (bpf_xdp_attach(ifindex, xdp_fd, xdp_flags, nullptr) < 0)
	{
		std::cerr << "Failed to attach XDP to " << iface << std::endl;
		return 1;
	}

	// Attach cgroup ingress/egress (default: root /sys/fs/cgroup to cover all processes)
	int cgroup_fd = -1;
	if (cgroup_path)
	{
		cgroup_fd = open(cgroup_path, O_RDONLY | O_CLOEXEC);
		if (cgroup_fd >= 0)
		{
			if (cg_ingress)
			{
				if (bpf_prog_attach(bpf_program__fd(cg_ingress), cgroup_fd, BPF_CGROUP_INET_INGRESS, 0) < 0)
					std::cerr << "Warning: failed to attach cgroup ingress at " << cgroup_path << std::endl;
			}
			if (cg_egress)
			{
				if (bpf_prog_attach(bpf_program__fd(cg_egress), cgroup_fd, BPF_CGROUP_INET_EGRESS, 0) < 0)
					std::cerr << "Warning: failed to attach cgroup egress at " << cgroup_path << std::endl;
			}
		}
		else
		{
			std::cerr << "Warning: cannot open cgroup path: " << cgroup_path << std::endl;
		}
	}

	// Get map fds
	MapFds mfds{};
	mfds.global_stats = bpf_object__find_map_fd_by_name(obj, "global_stats");
	mfds.pid_stats = bpf_object__find_map_fd_by_name(obj, "pid_stats");
	mfds.cgroup_stats = bpf_object__find_map_fd_by_name(obj, "cgroup_stats");
	mfds.conn_stats = bpf_object__find_map_fd_by_name(obj, "conn_stats");
	mfds.cookie_proc_map = bpf_object__find_map_fd_by_name(obj, "cookie_proc_map");

	std::cout << "BPF loaded. Press Ctrl-C to stop.\n";
	while (!stop)
	{
		print_stats(mfds);
		sleep(2);
	}

	// Detach
	bpf_xdp_detach(ifindex, xdp_flags, nullptr);
	if (cgroup_fd >= 0)
	{
		// bpf_link isn't used; try detach via prog_detach
		if (cg_ingress)
			bpf_prog_detach2(bpf_program__fd(cg_ingress), cgroup_fd, BPF_CGROUP_INET_INGRESS);
		if (cg_egress)
			bpf_prog_detach2(bpf_program__fd(cg_egress), cgroup_fd, BPF_CGROUP_INET_EGRESS);
		close(cgroup_fd);
	}
	bpf_object__close(obj);
	return 0;
}
