//
// Created by usr on 09/10/2025.
//

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <tuple>
#include <unistd.h>
#include <linux/bpf.h>

struct bpf_object;
struct bpf_program;
struct bpf_link;

struct WFdCloser
{
	void operator()(int const* Fd) const
	{
		if (Fd && *Fd >= 0)
		{
			close(*Fd);
			delete Fd;
		}
	}
};

class WEbpfObj
{
protected:
	std::vector<std::tuple<bpf_program*, bpf_attach_type>> Programs{}; // attached programs
	std::vector<std::tuple<bpf_link*, bpf_program*>>       Links{};

	bpf_object*                     Obj{};
	std::unique_ptr<int, WFdCloser> CGroupFd{};
	int                             XdpIfIndex{};

public:
	explicit WEbpfObj();

	~WEbpfObj();

	bool FindAndAttachPlainProgram(std::string const& ProgName);

	bool FindAndAttachProgram(std::string const& ProgName, bpf_attach_type AttachType, unsigned int Flags = 0);

	bool FindAndAttachXdpProgram(std::string const& ProgName, int IfIndex, unsigned int Flags = 0);

	[[nodiscard]] int FindMapFd(std::string const& MapFdPath) const;

	operator bpf_object*() const { return Obj; }
};
