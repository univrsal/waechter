/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "IP2AsnDB.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <optional>

bool ParseIPRange(std::string const& Line, WIPIndexEntry& Entry)
{
	size_t FirstTab = Line.find('\t');
	if (FirstTab == std::string::npos)
	{
		return false;
	}

	size_t SecondTab = Line.find('\t', FirstTab + 1);
	if (SecondTab == std::string::npos)
	{
		return false;
	}

	std::string StartStr = Line.substr(0, FirstTab);
	std::string EndStr = Line.substr(FirstTab + 1, SecondTab - FirstTab - 1);

	auto Start = WIPAddress::FromString(StartStr);
	auto End = WIPAddress::FromString(EndStr);

	if (!Start || !End)
	{
		return false;
	}

	Entry.RangeStart = *Start;
	Entry.RangeEnd = *End;

	return true;
}

std::optional<WIP2AsnLookupResult> WIP2AsnDB::ReadEntryAtOffset(std::streampos Offset) const
{
	std::ifstream File(DatabasePath);
	if (!File.is_open())
	{
		return std::nullopt;
	}

	File.seekg(Offset);

	std::string Line;
	if (!std::getline(File, Line))
	{
		return std::nullopt;
	}

	// Parse the full line now
	std::istringstream  Iss(Line);
	std::string         StartStr, EndStr, AsnStr;
	WIP2AsnLookupResult Result;

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

	auto ParseResult = std::from_chars(EndStr.data(), EndStr.data() + EndStr.size(), Result.ASN);
	if (ParseResult.ec != std::errc())
	{
		Result.ASN = 0;
	}

	return Result;
}

bool WIP2AsnDB::Parse()
{
	std::ifstream File(DatabasePath);
	if (!File.is_open())
	{
		return false;
	}

	std::string Line;
	Index.reserve(650000);

	while (File)
	{
		std::streampos Offset = File.tellg();
		if (!std::getline(File, Line))
		{
			break;
		}

		if (Line.empty())
		{
			continue;
		}

		WIPIndexEntry Entry;
		Entry.FileOffset = Offset;

		if (ParseIPRange(Line, Entry))
		{
			Index.push_back(Entry);
		}
	}

	std::sort(Index.begin(), Index.end());
	Index.shrink_to_fit();
	return true;
}

std::optional<WIP2AsnLookupResult> WIP2AsnDB::Lookup(std::string const& IPStr)
{
	auto IP = WIPAddress::FromString(IPStr);
	if (!IP)
	{
		return std::nullopt;
	}

	auto It = std::upper_bound(Index.begin(), Index.end(), *IP,
		[](WIPAddress const& Addr, WIPIndexEntry const& Entry) { return Addr < Entry.RangeStart; });

	if (It == Index.begin())
	{
		return std::nullopt;
	}

	--It;

	if (*IP >= It->RangeStart && *IP <= It->RangeEnd)
	{
		return ReadEntryAtOffset(It->FileOffset);
	}

	return std::nullopt;
}