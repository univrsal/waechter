/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "LibCurl.hpp"

#include <dlfcn.h>
#include <string>
#include <cstdio>
#include <spdlog/spdlog.h>

namespace
{
	constexpr int CURLOPT_URL = 10002;
	constexpr int CURLOPT_WRITEFUNCTION = 20011;
	constexpr int CURLOPT_WRITEDATA = 10001;
	constexpr int CURLOPT_USERAGENT = 10018;
	constexpr int CURLOPT_FOLLOWLOCATION = 52;
	constexpr int CURLOPT_MAXREDIRS = 68;
	constexpr int CURLOPT_TIMEOUT = 13;            // seconds
	constexpr int CURLOPT_CONNECTTIMEOUT = 78;     // seconds
	constexpr int CURLOPT_ACCEPT_ENCODING = 10102; // "" to enable all supported
	constexpr int CURLOPT_NOPROGRESS = 43;
	constexpr int CURLOPT_XFERINFOFUNCTION = 20219;
	constexpr int CURLOPT_XFERINFODATA = 10220;
	constexpr int CURLE_OK = 0;

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
		auto* OnProgress = reinterpret_cast<std::function<void(float)>*>(Client);
		if (OnProgress && *OnProgress && DlTotal > 0)
		{
			(*OnProgress)(static_cast<float>(DlNow) / static_cast<float>(DlTotal));
		}
		return 0;
	}
} // namespace

void WLibCurl::Load()
{
	// Try common libcurl sonames
	char const* candidates[] = {
		"libcurl.so.4",
		"libcurl.so",
	};

	for (auto* name : candidates)
	{
		Handle = dlopen(name, RTLD_LAZY | RTLD_LOCAL);
		if (Handle)
			break;
	}

	if (!Handle)
	{
		spdlog::warn("LibCurl: failed to load libcurl: {}. No ip lookup.", dlerror());
		return;
	}

	Resolve();

	if (IsLoaded())
	{
		spdlog::info("libcurl version {} loaded", GetLoadedVersion());
	}
	else
	{
		spdlog::warn("LibCurl: libcurl loaded but required symbols are missing");
		dlclose(Handle);
		Handle = nullptr;
	}
}

WLibCurl::~WLibCurl()
{
	if (Handle)
	{
		dlclose(Handle);
		Handle = nullptr;
	}
}

void WLibCurl::Resolve()
{
	if (!Handle)
		return;
#define RESOLVE(sym) sym##_fp = reinterpret_cast<sym##_t>(dlsym(Handle, #sym))
	RESOLVE(curl_easy_init);
	RESOLVE(curl_easy_cleanup);
	RESOLVE(curl_easy_perform);
	RESOLVE(curl_easy_setopt);
	RESOLVE(curl_easy_strerror);
	RESOLVE(curl_version);
#undef RESOLVE
}

bool WLibCurl::IsLoaded() const noexcept
{
	return Handle && curl_easy_init_fp && curl_easy_cleanup_fp && curl_easy_perform_fp && curl_easy_setopt_fp
		&& curl_easy_strerror_fp;
}

std::string WLibCurl::GetLoadedVersion() const
{
	if (!IsLoaded())
		return {};
	if (curl_version_fp)
	{
		char const* Version = curl_version_fp();
		if (Version)
			return {Version};
	}
	return {};
}

WJson WLibCurl::GetJson(std::string const& Url, std::string& OutError) const
{
	OutError.clear();
	if (!IsLoaded())
	{
		OutError = "libcurl not loaded";
		return {};
	}

	CURL* Curl = curl_easy_init_fp();
	if (!Curl)
	{
		OutError = "curl_easy_init failed";
		return {};
	}

	std::string Body;

	auto SetOpt = [&](int Opt, auto Val) -> bool {
		auto code = curl_easy_setopt_fp(Curl, Opt, Val);
		if (code != CURLE_OK)
		{
			OutError = curl_easy_strerror_fp ? curl_easy_strerror_fp(code) : "curl_easy_setopt failed";
			return false;
		}
		return true;
	};

	bool bOk = SetOpt(CURLOPT_URL, Url.c_str())
		&& SetOpt(CURLOPT_WRITEFUNCTION, WriteCallback)
		&& SetOpt(CURLOPT_WRITEDATA, &Body) && SetOpt(CURLOPT_USERAGENT, "waechter/1.0 (+https://github.com/)")
		&& SetOpt(CURLOPT_FOLLOWLOCATION, 1L) && SetOpt(CURLOPT_MAXREDIRS, 5L) && SetOpt(CURLOPT_TIMEOUT, 10L)
		&& SetOpt(CURLOPT_CONNECTTIMEOUT, 5L) && SetOpt(CURLOPT_ACCEPT_ENCODING, ""); // enable all supported encodings

	if (!bOk)
	{
		curl_easy_cleanup_fp(Curl);
		return {};
	}

	CURLcode ReturnCode = curl_easy_perform_fp(Curl);
	if (ReturnCode != CURLE_OK)
	{
		OutError = curl_easy_strerror_fp ? curl_easy_strerror_fp(ReturnCode) : "curl_easy_perform failed";
		curl_easy_cleanup_fp(Curl);
		return {};
	}

	curl_easy_cleanup_fp(Curl);

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
	std::function<void(float)> OnProgress, std::function<void(std::string const&)> const& OnError) const
{
	if (!IsLoaded())
	{
		OnError("libcurl not loaded");
		return;
	}

	CURL* Curl = curl_easy_init_fp();
	if (!Curl)
	{
		OnError("curl_easy_init failed");
		return;
	}

	auto File = std::fopen(DestinationPath.string().c_str(), "wb");
	if (!File)
	{
		OnError("fopen failed");
		curl_easy_cleanup_fp(Curl);
		return;
	}

	auto SetOpt = [&](int Opt, auto Val) -> bool {
		auto code = curl_easy_setopt_fp(Curl, Opt, Val);
		if (code != CURLE_OK)
		{
			OnError(curl_easy_strerror_fp ? curl_easy_strerror_fp(code) : "curl_easy_setopt failed");
			return false;
		}
		return true;
	};

	bool bOk = SetOpt(CURLOPT_URL, Url.c_str()) && SetOpt(CURLOPT_WRITEFUNCTION, WriteFileCallback)
		&& SetOpt(CURLOPT_WRITEDATA, File) && SetOpt(CURLOPT_USERAGENT, "waechter/1.0 (+https://github.com/)")
		&& SetOpt(CURLOPT_FOLLOWLOCATION, 1L) && SetOpt(CURLOPT_MAXREDIRS, 5L) && SetOpt(CURLOPT_TIMEOUT, 30L)
		&& SetOpt(CURLOPT_CONNECTTIMEOUT, 10L);

	if (bOk && OnProgress)
	{
		bOk = SetOpt(CURLOPT_NOPROGRESS, 0L) && SetOpt(CURLOPT_XFERINFOFUNCTION, ProgressCallback)
			&& SetOpt(CURLOPT_XFERINFODATA, &OnProgress);
	}

	if (!bOk)
	{
		std::fclose(File);
		std::remove(DestinationPath.string().c_str());
		curl_easy_cleanup_fp(Curl);
		return;
	}

	CURLcode ReturnCode = curl_easy_perform_fp(Curl);
	std::fclose(File);

	if (ReturnCode != CURLE_OK)
	{
		OnError(curl_easy_strerror_fp ? curl_easy_strerror_fp(ReturnCode) : "curl_easy_perform failed");
		std::remove(DestinationPath.string().c_str());
	}

	curl_easy_cleanup_fp(Curl);
}
