/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "Settings.hpp"

#include "DbusUtil.hpp"

#include <algorithm>
#include <cereal/archives/json.hpp>
#include <fstream>
#include <spdlog/spdlog.h>

static std::string GetSettingsFilename()
{
	stdfs::path Base = WSettings::GetConfigFolder();
	if (Base.empty())
	{
		return "waechter.json";
	}
	return (Base / "waechter.json").string();
}

WSettings::WSettings()
{
	if (WFilesystem::Exists(GetSettingsFilename()))
	{
		Load();
	}
	else if (!WFilesystem::Writable(WSettings::GetConfigFolder()))
	{
		spdlog::warn("Config folder '{}' is not writable; settings will not be saved", GetConfigFolder().string());
	}
	else
	{
		bFirstRun = true;
		bUseDarkTheme = WDbusUtil::IsUsingDarkTheme();
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
}

void WSettings::AddToSocketPathHistory(std::string const& Path)
{
	// Remove if already exists
	auto It = std::find(SocketPathHistory.begin(), SocketPathHistory.end(), Path);
	if (It != SocketPathHistory.end())
	{
		SocketPathHistory.erase(It);
	}
	// Add to front
	SocketPathHistory.insert(SocketPathHistory.begin(), Path);

	// Keep only the last 10 entries
	if (SocketPathHistory.size() > 10)
	{
		SocketPathHistory.resize(10);
	}

	Save();
}
