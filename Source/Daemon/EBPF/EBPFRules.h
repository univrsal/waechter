//
// Created by usr on 20/11/2025.
//

#pragma once
#include "EBPFCommon.h"

struct
{
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, __u64); // Socket cookie
	__type(value, struct WSocketRules);
	__uint(max_entries, 0xffff);
} socket_rules SEC(".maps");
