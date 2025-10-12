//
// Created by usr on 12/10/2025.
//

#pragma once

#include <string>

#include "EbpfObj.hpp"

enum class EEbpfInitResult
{
	SUCCESS = 0,
	OPEN_FAILED,
	LOAD_FAILED,
	XDP_ATTACH_FAILED,
	CG_INGRESS_ATTACH_FAILED,
	CG_EGRESS_ATTACH_FAILED
};

class WWaechterEpf : public WEbpfObj
{
	int InterfaceIndex{-1};
public:

	explicit WWaechterEpf(int InterfaceIndex = -1, std::string const& ProgramObectFilePath = "ebpf/waechter_bpf.o");

	EEbpfInitResult Init();

};
