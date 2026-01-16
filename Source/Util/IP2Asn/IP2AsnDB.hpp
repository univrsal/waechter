/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <filesystem>
#include <utility>
#include <vector>
#include <optional>

#include "IPAddress.hpp"
#include "Singleton.hpp"

struct WIP2AsnLookupResult
{
	uint32_t    ASN;
	std::string Country;
	std::string Organization;
};

struct WIPIndexEntry
{
	WIPAddress     RangeStart;
	WIPAddress     RangeEnd;
	std::streampos FileOffset; // Offset to the line in the file

	bool operator<(WIPIndexEntry const& Other) const { return RangeStart < Other.RangeStart; }
};

class WIP2AsnDB
{
	std::filesystem::path                            DatabasePath;
	std::vector<WIPIndexEntry>                       Index;
	[[nodiscard]] std::optional<WIP2AsnLookupResult> ReadEntryAtOffset(std::streampos offset) const;

public:
	explicit WIP2AsnDB(std::filesystem::path Path) : DatabasePath(std::move(Path)) {}

	bool Parse();

	std::optional<WIP2AsnLookupResult> Lookup(std::string const& IPStr);

	[[nodiscard]] std::size_t GetSize() const { return Index.size(); }

	[[nodiscard]] std::size_t MemoryUsage() const { return GetSize() * sizeof(WIPIndexEntry); }
};