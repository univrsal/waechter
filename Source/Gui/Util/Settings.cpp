/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "Settings.hpp"

#include <algorithm>
#include <fstream>

#include "spdlog/spdlog.h"
#include "cereal/archives/json.hpp"

#include "SysUtil.hpp"

static std::string GetSettingsFilename()
{
	stdfs::path Base = WSysUtil::GetConfigFolder();
	if (Base.empty())
	{
		return "waechter.json";
	}
	return (Base / "waechter.json").string();
}

WSettings::WSettings()
{
	WSysUtil::LoadFilesystemFromIndexedDB();
	if (WFilesystem::Exists(GetSettingsFilename()))
	{
		Load();
	}
	else if (!WFilesystem::Writable(WSysUtil::GetConfigFolder()))
	{
		spdlog::warn(
			"Config folder '{}' is not writable; settings will not be saved", WSysUtil::GetConfigFolder().string());
	}
	else
	{
		bFirstRun = true;

		bUseDarkTheme = WSysUtil::IsUsingDarkTheme();
		spdlog::info("Creating default settings");
		Save();
	}
}

void WSettings::Load()
{
	std::string   Path = GetSettingsFilename();
	std::ifstream ifs(Path, std::ios::in | std::ios::binary);
	if (!ifs.is_open())
	{
		spdlog::warn("Settings file '{}' not found; using defaults", Path);
		return;
	}
	try
	{
		cereal::JSONInputArchive ar(ifs);
		ar(*this);
		spdlog::debug("Loaded settings from '{}'", Path);
	}
	catch (std::exception const& e)
	{
		spdlog::error("Failed to load settings from '{}': {}", Path, e.what());
	}
}

void WSettings::Save()
{
	std::string   Path = GetSettingsFilename();
	std::ofstream ofs(Path, std::ios::out | std::ios::binary | std::ios::trunc);
	if (!ofs.is_open())
	{
		spdlog::error("Failed to open settings file '{}' for writing", Path);
		return;
	}
	try
	{
		cereal::JSONOutputArchive ar(ofs);
		ar(*this);
		spdlog::debug("Saved settings to '{}'", Path);
	}
	catch (std::exception const& e)
	{
		spdlog::error("Failed to save settings to '{}': {}", Path, e.what());
	}
	WSysUtil::SyncFilesystemToIndexedDB();
}

void WSettings::AddToSocketPathHistory(std::string const& Path)
{
	if (Path.empty())
	{
		return;
	}

	// Remove if already exists
	auto const It = std::ranges::find(SocketPathHistory, Path);
	if (It != SocketPathHistory.end())
	{
		SocketPathHistory.erase(It);
	}

	// Add to the front by creating a new vector
	std::vector<std::string> NewHistory;
	NewHistory.reserve(11); // Reserve space for new entry + up to 10 existing
	NewHistory.push_back(Path);

	// Copy existing entries (up to 9 more to make total of 10)
	for (size_t i = 0; i < SocketPathHistory.size() && i < 9; ++i)
	{
		NewHistory.push_back(SocketPathHistory[i]);
	}

	SocketPathHistory = std::move(NewHistory);

	Save();
}
