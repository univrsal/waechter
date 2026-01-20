/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "Singleton.hpp"
#include "IP2Asn/IP2AsnDB.hpp"

class WIP2Asn : public TSingleton<WIP2Asn>
{
	bool bHaveDatabaseDownloaded{ false };
	std::atomic<bool> bUpdateInProgress{ false };
	std::thread       DownloadThread{};
	std::mutex        DownloadMutex{};
	std::atomic<float> DownloadProgress{ 0.0f };

	std::unique_ptr<WIP2AsnDB> Database{};

	static bool ExtractDatabase(std::filesystem::path const& GzPath, std::filesystem::path const& OutPath);

	std::unordered_map<std::string, std::optional<WIP2AsnLookupResult>> Cache{};

public:
	WIP2Asn() = default;
	~WIP2Asn() override
	{
		if (DownloadThread.joinable())
		{
			DownloadThread.join();
		}
	}
	void Init();

	void UpdateDatabase();

	void DrawDownloadProgressWindow();

	bool IsUpdateInProgress() const noexcept { return bUpdateInProgress.load(); }
	bool HasDatabase() const noexcept { return Database != nullptr; }

	std::optional<WIP2AsnLookupResult> Lookup(std::string const& IpAddress);
};
