/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "IP2Asn.hpp"

#include <cstdio>
#include <zlib.h>

#include "spdlog/spdlog.h"
#include "tracy/Tracy.hpp"

#include "Format.hpp"
#include "LibCurl.hpp"
#include "Filesystem.hpp"

static stdfs::path GetDataFolder()
{
	if (WFilesystem::Writable("/var/lib/waechter"))
	{
		return { "/var/lib/waechter" };
	}
	return { "." };
}

bool WIP2Asn::ExtractDatabase(std::filesystem::path const& GzPath, std::filesystem::path const& OutPath)
{
	auto const InputFile = gzopen(GzPath.string().c_str(), "rb");
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

	constexpr std::size_t BufferSize = 128 * 1024;
	auto const            Buffer = std::make_unique<char[]>(BufferSize);
	int                   BytesRead;

	while ((BytesRead = gzread(InputFile, static_cast<void*>(Buffer.get()), 128 * 1024)) > 0)
	{
		if (std::fwrite(static_cast<void*>(Buffer.get()), 1, static_cast<size_t>(BytesRead), OutputFile)
			!= static_cast<size_t>(BytesRead))
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

void WIP2Asn::LookupAddress(WQueuedRequest const& Request)
{
	if (!Database || bUpdateInProgress)
	{
		Request.Promise.Finish(std::nullopt);
		return;
	}

	if (auto const It = Cache.find(Request.AddressToResolve); It != Cache.end())
	{
		Request.Promise.Finish(It->second);
		return;
	}
	spdlog::debug("Looking up ASN for {}", Request.AddressToResolve.ToString());
	std::scoped_lock Lock(DownloadMutex);
	auto             Result = Database->Lookup(Request.AddressToResolve);
	if (!Result)
	{
		Cache[Request.AddressToResolve] = std::nullopt;
		Request.Promise.Finish(std::nullopt);
		spdlog::warn("IP2ASN lookup failed for address: {}", Request.AddressToResolve.ToString());
		return;
	}
	Result->Address = Request.AddressToResolve;

	auto CountryLowerCase = Result->Country;
	std::ranges::transform(CountryLowerCase, CountryLowerCase.begin(), [](unsigned char c) { return std::tolower(c); });
	Result->Country = CountryLowerCase;
	Cache[Request.AddressToResolve] = Result;
	Request.Promise.Finish(Result);
}

void WIP2Asn::LookupThreadFunc()
{
	static std::string Empty = {};
	pthread_setname_np(pthread_self(), "resolver");
	tracy::SetThreadName("ResolverThread");
	while (bRunning)
	{
		std::unique_lock Lock(QueueMutex);
		QueueCondition.wait(Lock, [this] { return !PendingAddresses.empty() || !bRunning; });

		if (!bRunning)
		{
			break;
		}
		spdlog::debug("Processing {} pending IP2ASN lookup requests", PendingAddresses.size());

		if (!PendingAddresses.empty())
		{
			auto Request = PendingAddresses.front(); // copy before pop to keep TPromise's WSharedState alive
			PendingAddresses.pop();

			if (Request.AddressToResolve.IsZero())
			{
				Request.Promise.Finish(std::nullopt);
			}
			else if (auto It = Cache.find(Request.AddressToResolve); It != Cache.end())
			{
				Request.Promise.Finish(It->second);
			}
			else
			{
				Lock.unlock();
				LookupAddress(Request);
			}
		}
	}
}

void WIP2Asn::Init()
{
	auto DatabasePath = GetDataFolder() / "ip2asn_db.tsv";
	Database = nullptr;
	if (std::filesystem::exists(DatabasePath))
	{
		Database = std::make_unique<WIP2AsnDB>(DatabasePath);
		if (Database->Init())
		{
			spdlog::info("Loaded IP2ASN database with {} entries (memory usage: {})", Database->GetSize(),
				WStorageFormat::AutoFormat(Database->MemoryUsage()));
			bHaveDatabaseDownloaded = true;
		}
		else
		{
			spdlog::error("Failed to parse IP2ASN database at {}", DatabasePath.string());
		}
	}
	else
	{
		spdlog::warn("IP2ASN database not found at {}. Download it through the client via Tools > Update IP2Asn DB",
			DatabasePath.string());
	}

	bRunning = true;
	LookupThread = std::thread(&WIP2Asn::LookupThreadFunc, this);
}

void WIP2Asn::Stop()
{
	bRunning = false;
	QueueCondition.notify_one();
	if (LookupThread.joinable())
	{
		LookupThread.join();
	}
}

void WIP2Asn::UpdateDatabase()
{
	if (bUpdateInProgress)
	{
		return;
	}
	if (DownloadThread.joinable())
	{
		DownloadThread.join();
	}
	static std::string const URL = "https://waechter.st/ip2asn-combined.tsv.gz";

	auto OldDatabasePath = GetDataFolder() / "ip2asn_db.tsv";
	if (std::filesystem::exists(OldDatabasePath))
	{
		std::filesystem::remove(OldDatabasePath);
	}
	OldDatabasePath = GetDataFolder() / "ip2asn_db.tsv.gz";
	if (std::filesystem::exists(OldDatabasePath))
	{
		std::filesystem::remove(OldDatabasePath);
	}
	OldDatabasePath = GetDataFolder() / "ip2asn_db.tsv.idx";
	if (std::filesystem::exists(OldDatabasePath))
	{
		std::filesystem::remove(OldDatabasePath);
	}

	bUpdateInProgress = true;
	DownloadThread = std::thread([this] {
		std::scoped_lock lock(DownloadMutex);
		auto const       DatabasePath = GetDataFolder() / "ip2asn_db.tsv.gz";
		WLibCurl::DownloadFile(
			URL, DatabasePath,
			[&](float const Progress) { spdlog::info("Downloading IP2ASN database: {:.2f}%", Progress * 100); },
			[](std::string const& Error) { spdlog::error("Failed to download IP2ASN database: {}", Error); });
		if (ExtractDatabase(DatabasePath, DatabasePath.parent_path() / "ip2asn_db.tsv"))
		{
			spdlog::info("Extracted IP2ASN database successfully");
			Init();
		}
		bUpdateInProgress = false;
	});
}

TPromise<std::optional<WIP2AsnLookupResult> const&> WIP2Asn::Lookup(WIPAddress const& IpAddress)
{
	TPromise<std::optional<WIP2AsnLookupResult> const&> Promise{};
	{
		WQueuedRequest const Request{ IpAddress, Promise };
		std::lock_guard      Lock(QueueMutex);
		PendingAddresses.push(Request);
		QueueCondition.notify_one();
	}

	return Promise;
}