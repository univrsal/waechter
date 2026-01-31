/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <string>
#include <vector>

#include "Types.hpp"

#define CALC_MAP_USAGE(Map, KeyType, ValueType) (Map.size() * (sizeof(KeyType) + sizeof(ValueType)))

struct WMemoryStatEntry
{
	std::string Name;
	WBytes      Usage;
};
struct WMemoryStat
{
	std::string                   Name;
	std::vector<WMemoryStatEntry> ChildEntries;
};

class IMemoryTrackable
{
public:
	IMemoryTrackable() = default;
	virtual ~IMemoryTrackable() {}

	virtual WMemoryStat GetMemoryUsage() = 0;
};