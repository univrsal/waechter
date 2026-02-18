/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <string>
#include <utility>
#include <vector>

class WAboutDialog
{
	bool bVisible{};

	struct WThirdPartyLibrary
	{
		std::string Name;
		std::string Url;
		std::string LicenseText;

		WThirdPartyLibrary(std::string Name_, std::string Url_, std::string LicenseText_)
			: Name(std::move(Name_)), Url(std::move(Url_)), LicenseText(std::move(LicenseText_))
		{
		}
	};

	std::string WaechterLicense{};

	std::vector<WThirdPartyLibrary> ThirdPartyLibraries;

	std::string VersionString;

public:
	WAboutDialog();

	void Draw();

	void Show() { bVisible = true; }
};
