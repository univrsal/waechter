/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "TrayIcon.hpp"

#include "traycon.h"
#include "stb_image.h"
#include "spdlog/spdlog.h"

#include "Assets.hpp"
#include "Settings.hpp"
#include "Windows/SdlWindow.hpp"

void WTrayIcon::OnClick(traycon* /*Tray*/, void* Userdata)
{
	// This callback is invoked from traycon's event thread.
	// SDL functions are generally not thread-safe, so we
	// just set a flag here and let Step() (called from Tick) do the work.
	auto* Self = static_cast<WTrayIcon*>(Userdata);
	auto* Window = WSdlWindow::GetInstance().GetWindow();
	if (!Window)
	{
		return;
	}

	int const Visible = (SDL_GetWindowFlags(Window) & SDL_WINDOW_SHOWN) ? 1 : 0;
	Self->PendingVisibility.store(Visible ? 0 : 1);
}

void WTrayIcon::Init()
{
	if (!WSettings::GetInstance().bEnableTrayIcon)
	{
		return;
	}

	int            Width{}, Height{}, Channels{};
	unsigned char* Pixels =
		stbi_load_from_memory(GIconData, static_cast<int>(GIconSize), &Width, &Height, &Channels, 4);

	if (!Pixels)
	{
		spdlog::error("TrayIcon: failed to decode icon image");
		return;
	}

	Traycon = traycon_create(Pixels, Width, Height, OnClick, this);
	stbi_image_free(Pixels);

	if (!Traycon)
	{
		spdlog::error("TrayIcon: traycon_create failed");
	}
}

void WTrayIcon::Destroy()
{
	traycon_destroy(Traycon);
	Traycon = nullptr;
}

void WTrayIcon::Step()
{
	if (!Traycon)
	{
		return;
	}

	// Apply any visibility toggle requested by the click callback
	int const Pending = PendingVisibility.exchange(-1);
	if (Pending == 0)
	{
		SDL_HideWindow(WSdlWindow::GetInstance().GetWindow());
	}
	else if (Pending == 1)
	{
		auto* Window = WSdlWindow::GetInstance().GetWindow();
		SDL_ShowWindow(Window);
		SDL_RaiseWindow(Window);
	}

	if (traycon_step(Traycon) < 0)
	{
		spdlog::warn("TrayIcon: traycon_step returned error, destroying tray icon");
		Destroy();
	}
}
