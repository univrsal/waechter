/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "IP2Asn.hpp"

#include <cstdio>
#include <zlib.h>

#include "spdlog/spdlog.h"
#include "imgui.h"

#include "Format.hpp"
#include "LibCurl.hpp"
#include "Settings.hpp"
#include "SysUtil.hpp"
#include "Windows/GlfwWindow.hpp"

bool WIP2Asn::ExtractDatabase(std::filesystem::path const& GzPath, std::filesystem::path const& OutPath)
{
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

	constexpr std::size_t   BufferSize = 128 * 1024;
	std::unique_ptr<char[]> Buffer = std::make_unique<char[]>(BufferSize);
	int                     BytesRead;

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

void WIP2Asn::Init()
{
	auto DatabasePath = WSysUtil::GetConfigFolder() / "ip2asn_db.tsv";
	Database = nullptr;
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
	if (DownloadThread.joinable())
	{
		DownloadThread.join();
	}
	static std::string const URL = "https://waechter.st/ip2asn-combined.tsv.gz";

	auto OldDatabasePath = WSysUtil::GetConfigFolder() / "ip2asn_db.tsv";
	if (std::filesystem::exists(OldDatabasePath))
	{
		std::filesystem::remove(OldDatabasePath);
	}
	OldDatabasePath = WSysUtil::GetConfigFolder() / "ip2asn_db.tsv.gz";
	if (std::filesystem::exists(OldDatabasePath))
	{
		std::filesystem::remove(OldDatabasePath);
	}
	OldDatabasePath = WSysUtil::GetConfigFolder() / "ip2asn_db.tsv.idx";
	if (std::filesystem::exists(OldDatabasePath))
	{
		std::filesystem::remove(OldDatabasePath);
	}

	bUpdateInProgress = true;
	DownloadThread = std::thread([this] {
		std::scoped_lock lock(DownloadMutex);
		auto DatabasePath = WSysUtil::GetConfigFolder() / "ip2asn_db.tsv.gz";
		WLibCurl::DownloadFile(
			URL, DatabasePath, [&](float Progress) { DownloadProgress = Progress; },
			[](std::string const& Error) { spdlog::error("Failed to download IP2ASN database: {}", Error); });
		if (ExtractDatabase(DatabasePath, DatabasePath.parent_path() / "ip2asn_db.tsv"))
		{
			spdlog::info("Extracted IP2ASN database successfully");
			Init();
		}
		bUpdateInProgress = false;
	});
}

void WIP2Asn::DrawDownloadProgressWindow() const
{
	if (!bUpdateInProgress)
	{
		return;
	}
	ImGuiIO& Io = ImGui::GetIO();
	auto     DisplaySize = Io.DisplaySize;
	ImGui::SetNextWindowPos(ImVec2(DisplaySize.x * 0.5f, DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(400, 80), ImGuiCond_Always);
	if (ImGui::Begin("Updating IP2Asn database", nullptr,
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking))
	{
		ImGui::ProgressBar(DownloadProgress.load(), ImVec2(-1.0f, 0.0f));
	}
	ImGui::End();
}

std::optional<WIP2AsnLookupResult> WIP2Asn::Lookup(std::string const& IpAddress)
{
	if (!Database || bUpdateInProgress)
	{
		return std::nullopt;
	}

	if (auto const It = Cache.find(IpAddress); It != Cache.end())
	{
		return It->second;
	}
	std::scoped_lock Lock(DownloadMutex);
	auto             Result = Database->Lookup(IpAddress);
	if (!Result)
	{
		Cache[IpAddress] = std::nullopt;
		return std::nullopt;
	}

	auto CountryLowerCase = Result->Country;
	std::ranges::transform(CountryLowerCase, CountryLowerCase.begin(), [](unsigned char c) { return std::tolower(c); });
	Result->Country = CountryLowerCase;
	Cache[IpAddress] = Result;
	return Result;
}