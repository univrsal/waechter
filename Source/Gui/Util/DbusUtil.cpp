/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "DbusUtil.hpp"

#include <cstdio>
#include <string>

#if _WIN32
	#include <windows.h>
#endif

bool WDbusUtil::IsUsingDarkTheme()
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

#else
	// todo
	return false;
#endif

	return false;
}