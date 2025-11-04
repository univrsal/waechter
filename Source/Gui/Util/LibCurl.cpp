//
// Created by usr on 04/11/2025.
//

#include "LibCurl.hpp"

#include <dlfcn.h>
#include <string>

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
	constexpr int CURLE_OK = 0;

	size_t WriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
	{
		auto* buf = reinterpret_cast<std::string*>(userdata);
		buf->append(ptr, size * nmemb);
		return size * nmemb;
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
	return Handle && curl_easy_init_fp && curl_easy_cleanup_fp && curl_easy_perform_fp && curl_easy_setopt_fp && curl_easy_strerror_fp;
}

std::string WLibCurl::GetLoadedVersion() const
{
	if (!IsLoaded())
		return {};
	if (curl_version_fp)
	{
		char const* v = curl_version_fp();
		if (v)
			return std::string(v);
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
		auto code = curl_easy_setopt_fp(Curl, static_cast<CURLoption>(Opt), Val);
		if (code != CURLE_OK)
		{
			OutError = curl_easy_strerror_fp ? curl_easy_strerror_fp(code) : "curl_easy_setopt failed";
			return false;
		}
		return true;
	};

	bool bOk =
		SetOpt(CURLOPT_URL, Url.c_str()) && SetOpt(CURLOPT_WRITEFUNCTION, reinterpret_cast<size_t (*)(char*, size_t, size_t, void*)>(WriteCallback)) && SetOpt(CURLOPT_WRITEDATA, &Body) && SetOpt(CURLOPT_USERAGENT, "waechter/1.0 (+https://github.com/)") && SetOpt(CURLOPT_FOLLOWLOCATION, 1L) && SetOpt(CURLOPT_MAXREDIRS, 5L) && SetOpt(CURLOPT_TIMEOUT, 10L) && SetOpt(CURLOPT_CONNECTTIMEOUT, 5L) && SetOpt(CURLOPT_ACCEPT_ENCODING, ""); // enable all supported encodings

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
