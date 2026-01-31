/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "SysUtil.hpp"

#include <string>

#if _WIN32
	#include <windows.h>
#endif

#if __EMSCRIPTEN__
	#include <emscripten/val.h>
	#include <emscripten.h>
#endif

bool WSysUtil::IsUsingDarkTheme()
{
#if _WIN32
	DWORD value = 0;
	DWORD size = sizeof(value);
	HKEY  key;

	if (RegOpenKeyExA(
			HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &key)
		== ERROR_SUCCESS)
	{
		RegQueryValueExA(key, "AppsUseLightTheme", nullptr, nullptr, (LPBYTE)&value, &size);

		RegCloseKey(key);
		return value == 0;
	}
#elif defined(__linux__)
	FILE* Pipe = popen("busctl --user call "
					   "org.freedesktop.portal.Desktop "
					   "/org/freedesktop/portal/desktop "
					   "org.freedesktop.portal.Settings Read ss "
					   "org.freedesktop.appearance color-scheme 2>/dev/null",
		"r");

	if (!Pipe)
	{
		return false;
	}

	char        Buffer[256];
	std::string Output;

	while (fgets(Buffer, sizeof(Buffer), Pipe))
	{
		Output += Buffer;
	}

	pclose(Pipe);

	// Output looks like: "u 2"
	if (Output.find(" 1") != std::string::npos)
	{
		return true;
	}
#elif defined(__EMSCRIPTEN__)

	auto matchMedia = emscripten::val::global("window")["matchMedia"];
	auto query = emscripten::val("prefers-color-scheme: dark");
	return matchMedia(query)["matches"].as<bool>();
#else
	// todo macos
#endif

	return false;
}

stdfs::path WSysUtil::GetConfigFolder()
{
#if __linux__
	char const* Xdg = std::getenv("XDG_CONFIG_HOME");
	stdfs::path Base;
	if (Xdg && Xdg[0] != '\0')
	{
		Base = stdfs::path(Xdg);
	}
	else
	{
		char const* Home = std::getenv("HOME");
		if (!Home || Home[0] == '\0')
		{
			spdlog::warn("Neither XDG_CONFIG_HOME nor HOME are set; cannot determine config folder");
			return {};
		}
		Base = stdfs::path(Home) / ".config";
	}
#elif EMSCRIPTEN
	stdfs::path Base = "/waechter_data";
#elif _WIN32
	wchar_t*    AppDataPath = nullptr;
	HRESULT     hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &AppDataPath);
	stdfs::path Base;
	if (SUCCEEDED(hr) && AppDataPath)
	{
		Base = stdfs::path(AppDataPath);
		CoTaskMemFree(AppDataPath);
	}
	else
	{
		char const* AppData = std::getenv("APPDATA");
		if (!AppData || AppData[0] == '\0')
		{
			spdlog::warn("Could not determine APPDATA folder; cannot determine config folder");
			return {};
		}
		Base = stdfs::path(AppData);
	}
#else
	char const* Home = std::getenv("HOME");
	if (!Home || Home[0] == '\0')
	{
		spdlog::warn("HOME is not set; cannot determine config folder");
		return {};
	}
	stdfs::path Base = stdfs::path(Home) / "Library" / "Application Support";
#endif
	stdfs::path Appdir = Base / "waechter";

	std::error_code ec;
	if (!stdfs::exists(Appdir))
	{
		if (!stdfs::create_directories(Appdir, ec))
		{
			spdlog::error("Failed to create config directory {}: {}", Appdir.string(), ec.message());
			return { "." };
		}
	}

	if (!WFilesystem::Writable(Appdir))
	{
		spdlog::warn("Config directory {} is not writable", Appdir.string());
		return { "." };
	}

	return Appdir;
}

void WSysUtil::SyncFilesystemToIndexedDB()
{
#if EMSCRIPTEN
	EM_ASM(FS.syncfs(
		false, function(err) {
			if (err)
			{
				console.error('Error syncing filesystem to IndexedDB:', err);
			}
			else
			{
				console.log('Filesystem synced to IndexedDB');
			}
		}););
#endif
}

// clang-format off
#if EMSCRIPTEN
EM_ASYNC_JS(void, load_filesystem_impl, (), {
	try
	{
		FS.mkdir('/waechter_data');
	}
	catch (e)
	{
		// Directory might already exist, ignore
	}
	FS.mount(IDBFS, {}, '/waechter_data');

	await new Promise((resolve, reject) => {
		FS.syncfs(
			true, (err) => {
				if (err)
				{
					console.error('Failed to load saved data:', err);
					reject(err);
				}
				else
				{
					console.log('Saved data loaded successfully');
					resolve();
				}
			});
	});
});
#endif
// clang-format on

void WSysUtil::LoadFilesystemFromIndexedDB()
{
#if EMSCRIPTEN
	load_filesystem_impl();
#endif
}
