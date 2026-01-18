/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "IP2Asn.hpp"

#include <cstdio>
#include <spdlog/spdlog.h>
#include <zlib.h>

#include "Format.hpp"
#include "LibCurl.hpp"
#include "Settings.hpp"
#include "Windows/GlfwWindow.hpp"

bool WIP2Asn::ExtractDatabase(std::filesystem::path const& GzPath, std::filesystem::path const& OutPath)
{
	// unzip the gz file with zlib
	gzFile InputFile = gzopen(GzPath.string().c_str(), "rb");
	if (!InputFile)
	{
		spdlog::error("Failed to open gzipped IP2ASN database at {}", GzPath.string());
		return false;
	}

	FILE* OutputFile = std::fopen(OutPath.string().c_str(), "wb");
	if (!OutputFile)
	{
		spdlog::error("Failed to open output IP2ASN database file at {}", OutPath.string());
		gzclose(InputFile);
		return false;
	}

	char Buffer[128 * 1024];
	int  BytesRead;
	while ((BytesRead = gzread(InputFile, Buffer, sizeof(Buffer))) > 0)
	{
		if (std::fwrite(Buffer, 1, static_cast<size_t>(BytesRead), OutputFile) != static_cast<size_t>(BytesRead))
		{
			spdlog::error("Failed to write to output file {}", OutPath.string());
			break;
		}
	}

	if (BytesRead < 0)
	{
		int         ErrNum;
		char const* ErrMsg = gzerror(InputFile, &ErrNum);
		spdlog::error("Failed to read from gzip file {}: {}", GzPath.string(), ErrMsg);
	}

	gzclose(InputFile);
	std::fclose(OutputFile);
	return true;
}

void WIP2Asn::Init()
{
	auto DatabasePath = WSettings::GetConfigFolder() / "ip2asn_db.tsv";

	if (std::filesystem::exists(DatabasePath))
	{
		Database = std::make_unique<WIP2AsnDB>(DatabasePath);
		if (Database->Init())
		{
			spdlog::info("Loaded IP2ASN database with {} entries (memory usage: {})", Database->GetSize(),
				WStorageFormat::AutoFormat(Database->MemoryUsage()));
			bHaveDatabaseDownloaded = true;
			return;
		}
		spdlog::error("Failed to parse IP2ASN database at {}", DatabasePath.string());

		return;
	}

	spdlog::warn("IP2ASN database not found at {}. Download it via Tools > Update IP2Asn DB", DatabasePath.string());
}

void WIP2Asn::UpdateDatabase()
{
	if (bUpdateInProgress)
	{
		return;
	}
	static std::string const URL = "https://waechter.st/ip2asn-combined.tsv.gz";
	DownloadMutex.lock();
	// remove old database files
	auto OldDatabasePath = WSettings::GetConfigFolder() / "ip2asn_db.tsv";
	if (std::filesystem::exists(OldDatabasePath))
	{
		std::filesystem::remove(OldDatabasePath);
	}
	OldDatabasePath = WSettings::GetConfigFolder() / "ip2asn_db.tsv.gz";
	if (std::filesystem::exists(OldDatabasePath))
	{
		std::filesystem::remove(OldDatabasePath);
	}
	OldDatabasePath = WSettings::GetConfigFolder() / "ip2asn_db.tsv.idx";
	if (std::filesystem::exists(OldDatabasePath))
	{
		std::filesystem::remove(OldDatabasePath);
	}
	bUpdateInProgress = true;
	DownloadThread = std::thread([this]() {
		auto const& cURL = WMainWindow::Get().GetLibCurl();
		auto        DatabasePath = WSettings::GetConfigFolder() / "ip2asn_db.tsv.gz";
		cURL.DownloadFile(
			URL, DatabasePath,
			[](float Progress) { spdlog::info("Downloading IP2ASN database... {:.2f}%", Progress * 100.0f); },
			[](std::string const& Error) { spdlog::error("Failed to download IP2ASN database: {}", Error); });
		if (ExtractDatabase(DatabasePath, DatabasePath.parent_path() / "ip2asn_db.tsv"))
		{
			spdlog::info("Extracted IP2ASN database successfully");
			Init();
		}
		DownloadMutex.unlock();
		bUpdateInProgress = false;
	});
}

std::optional<WIP2AsnLookupResult> WIP2Asn::Lookup(std::string const& IpAddress)
{
	if (!Database)
	{
		return std::nullopt;
	}

	if (auto It = Cache.find(IpAddress); It != Cache.end())
	{
		return It->second;
	}
	std::scoped_lock Lock(DownloadMutex);
	auto             Result = Database->Lookup(IpAddress);

	auto CountryLowerCase = Result->Country;
	std::ranges::transform(CountryLowerCase, CountryLowerCase.begin(), [](unsigned char c) { return std::tolower(c); });
	Result->Country = CountryLowerCase;
	Cache[IpAddress] = Result;
	return Result;
}