/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "LibCurl.hpp"

#include <curl/curl.h>
#include <curl/curlver.h>
#include <string>
#include <cstdio>

#include "spdlog/spdlog.h"

namespace
{

	size_t WriteCallback(char* Ptr, size_t Size, size_t NMemB, void* UserData)
	{
		auto* Buf = reinterpret_cast<std::string*>(UserData);
		Buf->append(Ptr, Size * NMemB);
		return Size * NMemB;
	}

	size_t WriteFileCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
	{
		return std::fwrite(ptr, size, nmemb, static_cast<FILE*>(userdata));
	}

	int ProgressCallback(void* Client, long long DlTotal, long long DlNow, long long, long long)
	{
		auto* OnProgress = static_cast<std::function<void(float)>*>(Client);
		if (OnProgress && *OnProgress && DlTotal > 0)
		{
			(*OnProgress)(static_cast<float>(DlNow) / static_cast<float>(DlTotal));
		}
		return 0;
	}

	template <class T>
	bool SetOpt(CURL* Curl, CURLoption Opt, T Val)
	{
		auto code = curl_easy_setopt(Curl, Opt, Val);
		if (code != CURLE_OK)
		{
			return false;
		}
		return true;
	}

	bool GetHttpResponseCode(CURL* Curl, long& OutCode)
	{
		OutCode = 0;
		if (!Curl)
		{
			return false;
		}
		return curl_easy_getinfo(Curl, CURLINFO_RESPONSE_CODE, &OutCode) == CURLE_OK;
	}

	std::string GetEffectiveUrl(CURL* Curl)
	{
		char* Url = nullptr;
		if (Curl && curl_easy_getinfo(Curl, CURLINFO_EFFECTIVE_URL, &Url) == CURLE_OK && Url)
		{
			return Url;
		}
		return {};
	}
} // namespace

void WLibCurl::Init()
{
	curl_global_init(CURL_GLOBAL_DEFAULT);
}

void WLibCurl::Deinit()
{
	curl_global_cleanup();
}

std::string WLibCurl::GetLoadedVersion()
{
	return fmt::format("{} ({})", LIBCURL_VERSION, LIBCURL_TIMESTAMP);
}

WJson WLibCurl::GetJson(std::string const& Url, std::string& OutError)
{
	OutError.clear();
	auto* Curl = curl_easy_init();
	if (!Curl)
	{
		OutError = "curl_easy_init failed";
		return {};
	}

	std::string Body;

	bool bOk = SetOpt(Curl, CURLOPT_URL, Url.c_str()) && SetOpt(Curl, CURLOPT_WRITEFUNCTION, WriteCallback)
		&& SetOpt(Curl, CURLOPT_WRITEDATA, &Body)
		&& SetOpt(Curl, CURLOPT_USERAGENT, "waechter/1.0 (+https://waechter.st)")
		&& SetOpt(Curl, CURLOPT_FOLLOWLOCATION, 1L) && SetOpt(Curl, CURLOPT_MAXREDIRS, 5L)
		&& SetOpt(Curl, CURLOPT_TIMEOUT, 10L) && SetOpt(Curl, CURLOPT_CONNECTTIMEOUT, 5L)
		&& SetOpt(Curl, CURLOPT_ACCEPT_ENCODING, ""); // enable all supported encodings

	if (!bOk)
	{
		OutError = "curl_easy_init failed";
		curl_easy_cleanup(Curl);
		return {};
	}

	CURLcode ReturnCode = curl_easy_perform(Curl);
	if (ReturnCode != CURLE_OK)
	{
		OutError = curl_easy_strerror(ReturnCode);
		curl_easy_cleanup(Curl);
		return {};
	}

	// Only accept a successful HTTP 200 response.
	long HttpCode = 0;
	GetHttpResponseCode(Curl, HttpCode);
	if (HttpCode != 200)
	{
		auto EffectiveUrl = GetEffectiveUrl(Curl);
		if (HttpCode == 0)
		{
			OutError = EffectiveUrl.empty() ? "non-HTTP response" : fmt::format("non-HTTP response ({})", EffectiveUrl);
		}
		else
		{
			OutError = EffectiveUrl.empty() ? fmt::format("HTTP {}", HttpCode)
											: fmt::format("HTTP {} ({})", HttpCode, EffectiveUrl);
		}
		curl_easy_cleanup(Curl);
		return {};
	}

	curl_easy_cleanup(Curl);

	std::string Error;
	auto        Json = WJson::parse(Body, Error);
	if (!Error.empty())
	{
		OutError = std::string("json parse error: ") + Error;
		return {};
	}
	return Json;
}

void WLibCurl::DownloadFile(std::string const& Url, std::filesystem::path const& DestinationPath,
	std::function<void(float)> OnProgress, std::function<void(std::string const&)> const& OnError)
{
	CURL* Curl = curl_easy_init();
	if (!Curl)
	{
		OnError("curl_easy_init failed");
		return;
	}

	auto File = std::fopen(DestinationPath.string().c_str(), "wb");
	if (!File)
	{
		OnError("fopen failed");
		curl_easy_cleanup(Curl);
		return;
	}

	bool bOk = SetOpt(Curl, CURLOPT_URL, Url.c_str()) && SetOpt(Curl, CURLOPT_WRITEFUNCTION, WriteFileCallback)
		&& SetOpt(Curl, CURLOPT_WRITEDATA, File)
		&& SetOpt(Curl, CURLOPT_USERAGENT, "waechter/1.0 (+https://github.com/)")
		&& SetOpt(Curl, CURLOPT_FOLLOWLOCATION, 1L) && SetOpt(Curl, CURLOPT_MAXREDIRS, 5L)
		&& SetOpt(Curl, CURLOPT_TIMEOUT, 30L) && SetOpt(Curl, CURLOPT_CONNECTTIMEOUT, 10L);

	if (bOk && OnProgress)
	{
		bOk = SetOpt(Curl, CURLOPT_NOPROGRESS, 0L) && SetOpt(Curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback)
			&& SetOpt(Curl, CURLOPT_XFERINFODATA, &OnProgress);
	}

	if (!bOk)
	{
		OnError("curl_easy_setopt failed");
		std::fclose(File);
		std::remove(DestinationPath.string().c_str());
		curl_easy_cleanup(Curl);
		return;
	}

	CURLcode ReturnCode = curl_easy_perform(Curl);
	std::fclose(File);

	long HttpCode = 0;
	GetHttpResponseCode(Curl, HttpCode);

	if (ReturnCode != CURLE_OK)
	{
		OnError(curl_easy_strerror(ReturnCode));
		std::remove(DestinationPath.string().c_str());
	}
	else if (HttpCode != 200)
	{
		auto EffectiveUrl = GetEffectiveUrl(Curl);
		if (HttpCode == 0)
		{
			OnError(EffectiveUrl.empty() ? "non-HTTP response" : fmt::format("non-HTTP response ({})", EffectiveUrl));
		}
		else
		{
			OnError(EffectiveUrl.empty() ? fmt::format("HTTP {}", HttpCode)
										 : fmt::format("HTTP {} ({})", HttpCode, EffectiveUrl));
		}
		std::remove(DestinationPath.string().c_str());
	}

	curl_easy_cleanup(Curl);
}
