/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "I18n.hpp"

#define INCBIN_PREFIX G
#include "incbin.h"
#include "Json.hpp"
#include "Settings.hpp"
#include "spdlog/spdlog.h"

#define INCLUDE_LANG(id) INCBIN(id, LANGUAGES_DIR #id ".json")
#define LOAD_LANG(id)                                                                    \
	else if (Code == #id)                                                                \
	{                                                                                    \
		JsonData = std::string(reinterpret_cast<char const*>(G##id##Data), G##id##Size); \
	}

INCLUDE_LANG(en_US);
INCLUDE_LANG(de_DE);

WTranslation WI18n::LoadTranslationFile(std::string const& Code)
{
	std::string JsonData{};
	if (Code == "en_US")
	{
		JsonData = std::string(reinterpret_cast<char const*>(Gen_USData), Gen_USSize);
	}

	LOAD_LANG(de_DE);

	if (JsonData.empty())
	{
		spdlog::error("Unknown language code '{}'", Code);
		return {};
	}

	std::string Error{};
	auto const  JsonObj = WJson::parse(JsonData, Error);
	if (!Error.empty() || !JsonObj.is_object())
	{
		spdlog::error("Failed to load language file for '{}': {}", Code, Error);
		return {};
	}
	WTranslation Translation{};
	for (auto const& [Key, Value] : JsonObj.object_items())
	{
		if (Value.is_string())
		{
			if (Key.starts_with("__"))
			{
				// non imgui text
				Translation[Key] = Value.string_value();
			}
			else
			{
				Translation[Key] = Value.string_value() + "##" + Key;
			}
		}
	}
	return Translation;
}

char const* WI18n::Tr(char const* Key)
{
	if (Key == nullptr)
	{
		return "(null)";
	}

	auto& Instance = GetInstance();
	auto  It = Instance.ActiveTranslation.find(Key);
	if (It != Instance.ActiveTranslation.end())
	{
		return It->second.c_str();
	}

	It = Instance.English.find(Key);
	if (It != Instance.English.end())
	{
		return It->second.c_str();
	}
	return Key;
}

void WI18n::Init()
{
	English = LoadTranslationFile("en_US");
	LoadLanguage(WSettings::GetInstance().SelectedLanguage);
}

void WI18n::LoadLanguage(std::string const& Code)
{
	if (ActiveLanguageCode == Code)
	{
		return;
	}

	auto Translation = LoadTranslationFile(Code);
	if (!Translation.empty())
	{
		ActiveLanguageCode = Code;
		ActiveTranslation = Translation;
	}
}