//
// Created by usr on 27/10/2025.
//

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

		WThirdPartyLibrary(std::string Name, std::string Url, const unsigned char LicenseText[], unsigned int LicenseTextLength)
			: Name(std::move(Name))
			, Url(std::move(Url))
			, LicenseText(std::string(reinterpret_cast<const char*>(LicenseText), LicenseTextLength))
		{
		}
	};

	std::vector<WThirdPartyLibrary> ThirdPartyLibraries;

	std::string VersionString;

public:
	WAboutDialog();

	void Draw();

	void Show()
	{
		bVisible = true;
	}
};
