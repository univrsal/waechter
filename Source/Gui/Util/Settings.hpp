/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <cereal/cereal.hpp>

#include "Singleton.hpp"
#include "Filesystem.hpp"
#include "Format.hpp"
#include "Windows/NetworkGraphWindow.hpp"

class WSettings : public TSingleton<WSettings>
{
public:
	bool bFirstRun{ false };
	bool bShowUninitalizedSockets{ false };
	bool bShowOfflineProcesses{ false };

	std::string RegisteredUsername{};
	std::string RegistrationSerialKey{};

	std::string SocketPath{ "/var/run/waechterd.sock" };
	std::string WebSocketAuthToken{};
	bool        bAllowSelfSignedCertificates{ false };
	bool        bSkipCertificateHostnameCheck{ false };

	bool         bUseDarkTheme{ false };
	float        NetworkGraphLineWidth{ 1.0f };
	int          NetworkGraphHistorySetting{ NGH_5Min };
	ETrafficUnit TrafficTreeUnitSetting{ TU_Auto };

	WSettings();
	~WSettings() override = default;

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(CEREAL_NVP(bShowUninitalizedSockets), CEREAL_NVP(RegisteredUsername), CEREAL_NVP(RegistrationSerialKey),
			CEREAL_NVP(SocketPath), CEREAL_NVP(WebSocketAuthToken), CEREAL_NVP(bShowOfflineProcesses),
			CEREAL_NVP(NetworkGraphLineWidth), CEREAL_NVP(NetworkGraphHistorySetting),
			CEREAL_NVP(TrafficTreeUnitSetting), CEREAL_NVP(bUseDarkTheme), CEREAL_NVP(bAllowSelfSignedCertificates),
			CEREAL_NVP(bSkipCertificateHostnameCheck));
	}

	void Load();
	void Save();

	static stdfs::path GetConfigFolder()
	{
		char const* Xdg = std::getenv("XDG_CONFIG_HOME");
		stdfs::path Base;
		if (Xdg && Xdg[0] != '\0')
		{
			Base = stdfs::path(Xdg);
		}
		else
		{
			char const* Home = std::getenv("HOME");
			if (!Home || Home[0] == '\0')
			{
				spdlog::warn("Neither XDG_CONFIG_HOME nor HOME are set; cannot determine config folder");
				return {};
			}
			Base = stdfs::path(Home) / ".config";
		}

		stdfs::path Appdir = Base / "waechter";

		std::error_code ec;
		if (!stdfs::exists(Appdir))
		{
			if (!stdfs::create_directories(Appdir, ec))
			{
				spdlog::error("Failed to create config directory {}: {}", Appdir.string(), ec.message());
				return { "." };
			}
		}

		if (!WFilesystem::Writable(Appdir))
		{
			spdlog::warn("Config directory {} is not writable", Appdir.string());
			return { "." };
		}

		return Appdir;
	}
};
