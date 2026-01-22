/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include "IPAddress.hpp"

struct WIP2AsnLookupResult
{
	uint32_t    ASN;
	std::string Country;
	std::string Organization;
};

#pragma pack(push, 1)
struct WIP2AsnIndexHeader
{
	uint32_t Magic{}; // "IP2A"
	uint16_t Version{};
	uint16_t EntrySizeV4{};
	uint16_t EntrySizeV6{};
	uint16_t Reserved{};
	uint64_t CountV4{};
	uint64_t CountV6{};
	uint64_t TSVSize{}; // so we can detect truncated DB
};

struct WIP2AsnIndexEntryV4
{
	uint32_t Start{};  // IPv4 address in host byte order
	uint32_t End{};    // inclusive
	uint64_t Offset{}; // byte offset in the TSV
};

struct WIP2AsnIndexEntryV6
{
	std::array<uint8_t, 16> Start{};
	std::array<uint8_t, 16> End{};
	uint64_t                Offset{}; // byte offset in the TSV
};
#pragma pack(pop)

struct WIP2AsnIndexView
{
	WIP2AsnIndexHeader const*  Header{};
	WIP2AsnIndexEntryV4 const* V4Entries{};
	WIP2AsnIndexEntryV6 const* V6Entries{};
};

class WIP2AsnDB
{
	std::filesystem::path DatabasePath;
	std::filesystem::path IndexPath;

	int              IndexFd{ -1 };
	void*            IndexMapping{ nullptr };
	size_t           IndexMappingSize{ 0 };
	WIP2AsnIndexView IndexView{};

	[[nodiscard]] std::optional<WIP2AsnLookupResult> ReadEntryAtOffset(uint64_t offset) const;

	[[nodiscard]] bool BuildIndex() const;
	bool MapIndex();
	void UnmapIndex();

	[[nodiscard]] std::optional<WIP2AsnLookupResult> LookupIPv4(uint32_t IP) const;
	[[nodiscard]] std::optional<WIP2AsnLookupResult> LookupIPv6(std::array<uint8_t, 16> const& IPBytes) const;

public:
	explicit WIP2AsnDB(std::filesystem::path Path);
	~WIP2AsnDB();

	bool Init();

	[[nodiscard]] std::optional<WIP2AsnLookupResult> Lookup(std::string const& IPStr) const;

	[[nodiscard]] std::size_t GetSize() const;

	[[nodiscard]] std::size_t MemoryUsage() const { return IndexMappingSize; }
};