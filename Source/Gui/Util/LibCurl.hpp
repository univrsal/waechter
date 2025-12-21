/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <string>

#include "Json.hpp"

class WLibCurl
{
	void* Handle{ nullptr };

	using CURL = void;
	using CURLcode = int;
	using CURLoption = int;

	using curl_easy_init_t = CURL* (*)();
	using curl_easy_cleanup_t = void (*)(CURL*);
	using curl_easy_perform_t = CURLcode (*)(CURL*);
	using curl_easy_setopt_t = CURLcode (*)(CURL*, CURLoption, ...);
	using curl_easy_strerror_t = char const* (*)(CURLcode);
	using curl_version_t = char const* (*)();

	curl_easy_init_t     curl_easy_init_fp{ nullptr };
	curl_easy_cleanup_t  curl_easy_cleanup_fp{ nullptr };
	curl_easy_perform_t  curl_easy_perform_fp{ nullptr };
	curl_easy_setopt_t   curl_easy_setopt_fp{ nullptr };
	curl_easy_strerror_t curl_easy_strerror_fp{ nullptr };
	curl_version_t       curl_version_fp{ nullptr };

	void Resolve();

public:
	void Load();
	WLibCurl() = default;
	~WLibCurl();

	// Non-copyable, movable
	WLibCurl(WLibCurl const&) = delete;
	WLibCurl& operator=(WLibCurl const&) = delete;
	WLibCurl(WLibCurl&&) noexcept = default;
	WLibCurl& operator=(WLibCurl&&) noexcept = default;

	// Returns true if libcurl was successfully loaded.
	bool IsLoaded() const noexcept;

	std::string GetLoadedVersion() const;

	// Perform a blocking HTTP GET to the given URL and parse the response body as JSON.
	WJson GetJson(std::string const& Url, std::string& OutError) const;
};
