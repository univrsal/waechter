/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "Filesystem.hpp"

class WSysUtil
{
public:
	static bool IsUsingDarkTheme();
	static stdfs::path GetConfigFolder();

	static void SyncFilesystemToIndexedDB();
	static void LoadFilesystemFromIndexedDB();
};
