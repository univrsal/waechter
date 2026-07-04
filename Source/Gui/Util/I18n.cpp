/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "I18n.hpp"

#include "spdlog/spdlog.h"

#include "Assets.hpp"
#include "Json.hpp"
#include "Settings.hpp"

#define LOAD_LANG(id)                                                                    \
	else if (Code == #id)                                                                \
	{                                                                                    \
		JsonData = std::string(reinterpret_cast<char const*>(G##id##Data), G##id##Size); \
	}

static void AddObject(
	WTranslation& Translation, std::string const& BreadCrumb, WJson const& Obj, bool const bForImGui = false)
{
	for (auto const& [Key, Value] : Obj.object_items())
	{
		if (Value.is_object())
		{
			AddObject(Translation, BreadCrumb + Key + ".", Value.object_items(), bForImGui);
		}
		else
		{
			std::string TranslatedValue = Value.string_value();
			if (bForImGui)
			{
				if (Key == "title")
				{
					TranslatedValue += "###" + (BreadCrumb + Key);
				}
				else
				{
					TranslatedValue += "##" + (BreadCrumb + Key);
				}
			}
			Translation[BreadCrumb + Key] = TranslatedValue;
		}
	}
}

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
		if (Key == "static")
		{
			AddObject(Translation, "", Value.object_items(), false);
		}
		else
		{
			AddObject(Translation, Key + ".", Value.object_items(), true);
		}
	}
	return Translation;
}

char const* WI18n::Tr(char const* Key)
{
	assert(Key != nullptr && strlen(Key) > 0);
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