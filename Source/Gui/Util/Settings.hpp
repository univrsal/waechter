//
// Created by usr on 16/11/2025.
//

#pragma once
#include <cereal/cereal.hpp>

#include "Singleton.hpp"
#include "Filesystem.hpp"

class WSettings : public TSingleton<WSettings>
{
public:
	bool bShowUninitalizedSockets{ false };

	std::string RegisteredUsername{};
	std::string RegistrationSerialKey{};

	WSettings();
	~WSettings() override = default;

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(
			CEREAL_NVP(bShowUninitalizedSockets), CEREAL_NVP(RegisteredUsername), CEREAL_NVP(RegistrationSerialKey));
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
