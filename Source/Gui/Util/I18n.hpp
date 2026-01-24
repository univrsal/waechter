/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <unordered_map>
#include <string>

#include "Singleton.hpp"

#define TR(id) WI18n::Tr(id)

using WTranslation = std::unordered_map<std::string, std::string>;

class WI18n final : public TSingleton<WI18n>
{
	std::string  ActiveLanguageCode;
	WTranslation English{}; // fallback, always present
	WTranslation ActiveTranslation{};

	static WTranslation LoadTranslationFile(std::string const& Code);

public:
	static char const* Tr(char const* Key);

	void Init();
	void LoadLanguage(std::string const& Code);
};
