/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "IP2Asn.hpp"

#include "Format.hpp"
#include "Settings.hpp"

void WIP2Asn::Init()
{
	auto DatabasePath = WSettings::GetConfigFolder() / "ip2asn_db.tsv";

	if (std::filesystem::exists(DatabasePath))
	{
		Database = std::make_unique<WIP2AsnDB>(DatabasePath);
		if (Database->Parse())
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