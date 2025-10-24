//
// Created by usr on 12/10/2025.
//

#pragma once

#include <string>
#include <memory>
#include <bpf/libbpf.h>

#include "EbpfObj.hpp"

class WEbpfData;

// Add back init result enum

enum class EEbpfInitResult
{
	OPEN_FAILED,
	LOAD_FAILED,
	XDP_ATTACH_FAILED,
	CG_INGRESS_ATTACH_FAILED,
	CG_EGRESS_ATTACH_FAILED,
	MAPS_NOT_FOUND,
	SUCCESS = 0,
};

class WWaechterEbpf : public WEbpfObj
{
	int InterfaceIndex{ -1 };

	std::shared_ptr<WEbpfData> Data{};

public:
	explicit WWaechterEbpf(int InterfaceIndex = -1, std::string const& ProgramObectFilePath = BPF_OBJECT_PATH);
	~WWaechterEbpf();

	EEbpfInitResult Init();

	std::shared_ptr<WEbpfData> GetData() { return Data; }

	void PrintStats();
	void UpdateData();
};
