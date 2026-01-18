/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "IP2AsnDB.hpp"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <optional>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <spdlog/spdlog.h>

namespace
{
	constexpr uint32_t kIndexMagic = 0x41503249; // "IP2A" little endian
	constexpr uint16_t kIndexVersion = 1;

	int CmpIPv6(std::array<uint8_t, 16> const& A, std::array<uint8_t, 16> const& B)
	{
		return std::memcmp(A.data(), B.data(), A.size());
	}

} // namespace

std::optional<WIP2AsnLookupResult> WIP2AsnDB::ReadEntryAtOffset(uint64_t Offset) const
{
	std::ifstream File(DatabasePath);
	if (!File.is_open())
	{
		return std::nullopt;
	}

	File.seekg(static_cast<std::streamoff>(Offset));

	std::string Line;
	if (!std::getline(File, Line))
	{
		return std::nullopt;
	}

	std::istringstream  Iss(Line);
	std::string         StartStr, EndStr, AsnStr;
	WIP2AsnLookupResult Result{};

	if (!std::getline(Iss, StartStr, '\t'))
	{
		return std::nullopt;
	}
	if (!std::getline(Iss, EndStr, '\t'))
	{
		return std::nullopt;
	}
	if (!std::getline(Iss, AsnStr, '\t'))
	{
		return std::nullopt;
	}
	if (!std::getline(Iss, Result.Country, '\t'))
	{
		return std::nullopt;
	}

	if (!std::getline(Iss, Result.Organization))
	{
		return std::nullopt;
	}

	auto ParseResult = std::from_chars(AsnStr.data(), AsnStr.data() + AsnStr.size(), Result.ASN);
	if (ParseResult.ec != std::errc())
	{
		Result.ASN = 0;
	}

	return Result;
}

WIP2AsnDB::WIP2AsnDB(std::filesystem::path Path) : DatabasePath(std::move(Path))
{
	IndexPath = DatabasePath;
	IndexPath += ".idx";
}

WIP2AsnDB::~WIP2AsnDB()
{
	UnmapIndex();
}

bool WIP2AsnDB::Init()
{
	if (!std::filesystem::exists(DatabasePath))
	{
		spdlog::error("IP2ASN TSV not found at {}", DatabasePath.string());
		return false;
	}

	if (std::filesystem::exists(IndexPath))
	{
		if (MapIndex())
		{
			if (IndexView.Header && IndexView.Header->Magic == kIndexMagic && IndexView.Header->Version == kIndexVersion
				&& IndexView.Header->TSVSize == std::filesystem::file_size(DatabasePath))
			{
				return true;
			}
			spdlog::warn("Existing IP2ASN index is stale or invalid, rebuilding");
		}
		UnmapIndex();
		std::filesystem::remove(IndexPath);
	}

	return BuildIndex() && MapIndex();
}

bool WIP2AsnDB::BuildIndex() const
{
	std::ifstream File(DatabasePath);
	if (!File.is_open())
	{
		spdlog::error("Failed to open IP2ASN TSV at {}", DatabasePath.string());
		return false;
	}

	std::vector<WIP2AsnIndexEntryV4> V4Entries;
	std::vector<WIP2AsnIndexEntryV6> V6Entries;
	V4Entries.reserve(650000);
	V6Entries.reserve(100000);

	std::string Line;
	while (File)
	{
		uint64_t Offset = static_cast<uint64_t>(File.tellg());
		if (!std::getline(File, Line))
		{
			break;
		}

		if (Line.empty())
		{
			continue;
		}

		size_t FirstTab = Line.find('\t');
		size_t SecondTab = FirstTab == std::string::npos ? std::string::npos : Line.find('\t', FirstTab + 1);
		if (FirstTab == std::string::npos || SecondTab == std::string::npos)
		{
			continue;
		}

		auto Start = WIPAddress::FromString(Line.substr(0, FirstTab));
		auto End = WIPAddress::FromString(Line.substr(FirstTab + 1, SecondTab - FirstTab - 1));
		if (!Start || !End)
		{
			continue;
		}

		if (Start->Family == EIPFamily::IPv4)
		{
			WIP2AsnIndexEntryV4 Entry{};
			Entry.Start = Start->ToInt();
			Entry.End = End->ToInt();
			Entry.Offset = Offset;
			V4Entries.push_back(Entry);
		}
		else
		{
			WIP2AsnIndexEntryV6 Entry{};
			Entry.Start = Start->Bytes;
			Entry.End = End->Bytes;
			Entry.Offset = Offset;
			V6Entries.push_back(Entry);
		}
	}

	std::ranges::sort(V4Entries, [](auto const& A, auto const& B) { return A.Start < B.Start; });
	std::ranges::sort(V6Entries, [](auto const& A, auto const& B) { return CmpIPv6(A.Start, B.Start) < 0; });

	WIP2AsnIndexHeader Header{};
	Header.Magic = kIndexMagic;
	Header.Version = kIndexVersion;
	Header.EntrySizeV4 = static_cast<uint16_t>(sizeof(WIP2AsnIndexEntryV4));
	Header.EntrySizeV6 = static_cast<uint16_t>(sizeof(WIP2AsnIndexEntryV6));
	Header.CountV4 = V4Entries.size();
	Header.CountV6 = V6Entries.size();
	Header.TSVSize = std::filesystem::file_size(DatabasePath);

	std::ofstream Out(IndexPath, std::ios::binary | std::ios::trunc);
	if (!Out.is_open())
	{
		spdlog::error("Failed to create IP2ASN index at {}", IndexPath.string());
		return false;
	}

	Out.write(reinterpret_cast<char*>(&Header), sizeof(Header));
	if (!V4Entries.empty())
	{
		Out.write(reinterpret_cast<char*>(V4Entries.data()),
			static_cast<long int>(V4Entries.size() * sizeof(WIP2AsnIndexEntryV4)));
	}
	if (!V6Entries.empty())
	{
		Out.write(reinterpret_cast<char*>(V6Entries.data()),
			static_cast<long int>(V6Entries.size() * sizeof(WIP2AsnIndexEntryV6)));
	}

	if (!Out.good())
	{
		spdlog::error("Failed to write IP2ASN index at {}", IndexPath.string());
		return false;
	}

	spdlog::info("Built IP2ASN index: {} IPv4 entries, {} IPv6 entries", Header.CountV4, Header.CountV6);
	return true;
}

bool WIP2AsnDB::MapIndex()
{
	UnmapIndex();

	IndexFd = ::open(IndexPath.c_str(), O_RDONLY);
	if (IndexFd < 0)
	{
		spdlog::error("Failed to open IP2ASN index at {}", IndexPath.string());
		return false;
	}

	struct stat st{};
	if (fstat(IndexFd, &st) != 0)
	{
		spdlog::error("Failed to stat IP2ASN index at {}", IndexPath.string());
		::close(IndexFd);
		IndexFd = -1;
		return false;
	}

	IndexMappingSize = static_cast<size_t>(st.st_size);
	IndexMapping = mmap(nullptr, IndexMappingSize, PROT_READ, MAP_PRIVATE, IndexFd, 0);
	if (IndexMapping == MAP_FAILED)
	{
		spdlog::error("Failed to mmap IP2ASN index at {}", IndexPath.string());
		IndexMapping = nullptr;
		::close(IndexFd);
		IndexFd = -1;
		IndexMappingSize = 0;
		return false;
	}

	auto* Header = static_cast<WIP2AsnIndexHeader const*>(IndexMapping);
	if (Header->Magic != kIndexMagic || Header->Version != kIndexVersion)
	{
		spdlog::error("IP2ASN index header invalid, rebuilding");
		return false;
	}

	size_t ExpectedSize = sizeof(WIP2AsnIndexHeader) + static_cast<size_t>(Header->CountV4) * Header->EntrySizeV4
		+ static_cast<size_t>(Header->CountV6) * Header->EntrySizeV6;
	if (ExpectedSize != IndexMappingSize)
	{
		spdlog::error("IP2ASN index size mismatch (expected {}, got {}), rebuilding", ExpectedSize, IndexMappingSize);
		return false;
	}

	IndexView.Header = Header;
	auto* Base = static_cast<char const*>(IndexMapping) + sizeof(WIP2AsnIndexHeader);
	IndexView.V4Entries = reinterpret_cast<WIP2AsnIndexEntryV4 const*>(Base);
	IndexView.V6Entries = reinterpret_cast<WIP2AsnIndexEntryV6 const*>(Base + Header->CountV4 * Header->EntrySizeV4);

	return true;
}

void WIP2AsnDB::UnmapIndex()
{
	if (IndexMapping)
	{
		munmap(IndexMapping, IndexMappingSize);
		IndexMapping = nullptr;
		IndexMappingSize = 0;
	}
	if (IndexFd >= 0)
	{
		::close(IndexFd);
		IndexFd = -1;
	}
	IndexView = {};
}

std::optional<WIP2AsnLookupResult> WIP2AsnDB::LookupIPv4(uint32_t IP) const
{
	if (!IndexView.Header || !IndexView.V4Entries)
	{
		return std::nullopt;
	}

	auto const Begin = IndexView.V4Entries;
	auto const End = Begin + IndexView.Header->CountV4;
	auto       It =
		std::upper_bound(Begin, End, IP, [](uint32_t value, WIP2AsnIndexEntryV4 const& e) { return value < e.Start; });

	if (It == Begin)
	{
		return std::nullopt;
	}
	--It;
	if (IP >= It->Start && IP <= It->End)
	{
		return ReadEntryAtOffset(It->Offset);
	}
	return std::nullopt;
}

std::optional<WIP2AsnLookupResult> WIP2AsnDB::LookupIPv6(std::array<uint8_t, 16> const& IPBytes) const
{
	if (!IndexView.Header || !IndexView.V6Entries)
	{
		return std::nullopt;
	}
	auto Begin = IndexView.V6Entries;
	auto End = Begin + IndexView.Header->CountV6;
	auto It = std::upper_bound(Begin, End, IPBytes,
		[](std::array<uint8_t, 16> const& value, WIP2AsnIndexEntryV6 const& e) { return CmpIPv6(value, e.Start) < 0; });
	if (It == Begin)
	{
		return std::nullopt;
	}
	--It;
	if (CmpIPv6(IPBytes, It->Start) >= 0 && CmpIPv6(IPBytes, It->End) <= 0)
	{
		return ReadEntryAtOffset(It->Offset);
	}
	return std::nullopt;
}

std::optional<WIP2AsnLookupResult> WIP2AsnDB::Lookup(std::string const& IPStr) const
{
	auto const IP = WIPAddress::FromString(IPStr);
	if (!IP)
	{
		return std::nullopt;
	}

	if (IP->Family == EIPFamily::IPv4)
	{
		return LookupIPv4(IP->ToInt());
	}
	return LookupIPv6(IP->Bytes);
}

std::size_t WIP2AsnDB::GetSize() const
{
	if (!IndexView.Header)
	{
		return 0;
	}
	return IndexView.Header->CountV4 + IndexView.Header->CountV6;
}
