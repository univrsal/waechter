/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "cereal/cereal.hpp"
// ReSharper disable once CppUnusedIncludeDirective
#include "cereal/types/vector.hpp"
#include "cereal/types/string.hpp"

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

	std::string SelectedLanguage{ "en_US" };

#if __linux__
	std::string              SocketPath{ "/var/run/waechterd.sock" };
#else
	std::string SocketPath{ "wss://example.com/ws" };
#endif
	std::vector<std::string> SocketPathHistory{};
	std::string              WebSocketAuthToken{};
	bool                     bAllowSelfSignedCertificates{ false };
	bool                     bSkipCertificateHostnameCheck{ false };

	bool         bUseDarkTheme{ false };
	bool         bReduceFrameRateWhenInactive{ true };
	float        NetworkGraphLineWidth{ 1.0f };
	int          NetworkGraphHistorySetting{ NGH_5Min };
	ETrafficUnit TrafficTreeUnitSetting{ TU_Auto };

	WSettings();
	~WSettings() override = default;

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(CEREAL_NVP(bShowUninitalizedSockets), CEREAL_NVP(RegisteredUsername), CEREAL_NVP(RegistrationSerialKey),
			CEREAL_NVP(SocketPath), CEREAL_NVP(SocketPathHistory), CEREAL_NVP(WebSocketAuthToken),
			CEREAL_NVP(bShowOfflineProcesses), CEREAL_NVP(NetworkGraphLineWidth),
			CEREAL_NVP(NetworkGraphHistorySetting), CEREAL_NVP(TrafficTreeUnitSetting), CEREAL_NVP(bUseDarkTheme),
			CEREAL_NVP(bAllowSelfSignedCertificates), CEREAL_NVP(bSkipCertificateHostnameCheck),
			CEREAL_NVP(SelectedLanguage), CEREAL_NVP(bReduceFrameRateWhenInactive));
	}

	void Load();
	void Save();
	void AddToSocketPathHistory(std::string const& Path);
};
