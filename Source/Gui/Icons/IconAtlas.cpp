/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "IconAtlas.hpp"

#include "spdlog/spdlog.h"

#include "Assets.hpp"

std::unordered_map<std::string, std::pair<ImVec2, ImVec2>> const ICON_ATLAS_UV = {
	{ "computer", std::make_pair(ImVec2(0.003906f, 0.003906f), ImVec2(0.128906f, 0.128906f)) },
	{ "downloadallow", std::make_pair(ImVec2(0.136719f, 0.003906f), ImVec2(0.261719f, 0.128906f)) },
	{ "downloadblock", std::make_pair(ImVec2(0.269531f, 0.003906f), ImVec2(0.394531f, 0.128906f)) },
	{ "downloadlimit", std::make_pair(ImVec2(0.402344f, 0.003906f), ImVec2(0.527344f, 0.128906f)) },
	{ "noicon", std::make_pair(ImVec2(0.535156f, 0.003906f), ImVec2(0.660156f, 0.128906f)) },
	{ "phone", std::make_pair(ImVec2(0.667969f, 0.003906f), ImVec2(0.792969f, 0.128906f)) },
	{ "placeholder", std::make_pair(ImVec2(0.800781f, 0.003906f), ImVec2(0.925781f, 0.128906f)) },
	{ "process", std::make_pair(ImVec2(0.003906f, 0.136719f), ImVec2(0.128906f, 0.261719f)) },
	{ "proxy", std::make_pair(ImVec2(0.136719f, 0.136719f), ImVec2(0.261719f, 0.261719f)) },
	{ "server", std::make_pair(ImVec2(0.269531f, 0.136719f), ImVec2(0.394531f, 0.261719f)) },
	{ "tor", std::make_pair(ImVec2(0.402344f, 0.136719f), ImVec2(0.527344f, 0.261719f)) },
	{ "uploadallow", std::make_pair(ImVec2(0.535156f, 0.136719f), ImVec2(0.660156f, 0.261719f)) },
	{ "uploadblock", std::make_pair(ImVec2(0.667969f, 0.136719f), ImVec2(0.792969f, 0.261719f)) },
	{ "uploadlimit", std::make_pair(ImVec2(0.800781f, 0.136719f), ImVec2(0.925781f, 0.261719f)) },
	{ "vpn", std::make_pair(ImVec2(0.003906f, 0.269531f), ImVec2(0.128906f, 0.394531f)) },
};

void WIconAtlas::Load()
{
	IconAtlas = WImageUtils::LoadImageFromMemoryRGBA8(GIconAtlasData, GIconAtlasSize);
	Logo = WImageUtils::LoadImageFromMemoryRGBA8(GIconData, GIconSize);
}

void WIconAtlas::Unload()
{
	IconAtlas.Unload();
	Logo.Unload();
}