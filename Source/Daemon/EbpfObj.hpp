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

struct WFdCloser
{
	void operator()(int* fd) const
	{
		if (fd && *fd >= 0)
		{
			close(*fd);
			delete fd;
		}
	}
};

class WEbpfObj
{
protected:
	std::vector<std::tuple<bpf_program*, bpf_attach_type>> Programs{}; // attached programs
	bpf_object* Obj {};
	std::unique_ptr<int, WFdCloser> CGroupFd {};
	int XdpIfIndex{};
public:

	explicit WEbpfObj(std::string ProgramObectFilePath);

	~WEbpfObj();

	bool Load();

	bool FindAndAttachProgram(const std::string& ProgName, bpf_attach_type AttachType, int Flags = 0);

	bool FindAndAttachXdpProgram(const std::string& ProgName, int IfIndex, int Flags = 0);

	[[nodiscard]] int FindMapFd(std::string const& MapFdPath) const;

	operator bpf_object *() const { return Obj; }
};
