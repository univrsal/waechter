/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <string>
#include <filesystem>
#include <functional>

#include "Json.hpp"

class WLibCurl
{
public:
	static void Init();
	static void Deinit();
	WLibCurl() = default;
	~WLibCurl() = default;

	static std::string GetLoadedVersion();

	// Perform a blocking HTTP GET to the given URL and parse the response body as JSON.
	static WJson GetJson(std::string const& Url, std::string& OutError);

	static void DownloadFile(std::string const& Url, std::filesystem::path const& DestinationPath,
		std::function<void(float)> OnProgress, std::function<void(std::string const&)> const& OnError);
};
