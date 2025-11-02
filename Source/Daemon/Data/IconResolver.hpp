//
// Created by usr on 02/11/2025.
//

#pragma once
#include "spdlog/spdlog.h"

#include <unordered_map>
#include <string>
#include <vector>

struct WIcon
{
	uint Width{};
	uint Height{};

	std::vector<unsigned char> Rgba{};
};

class WIconResolver
{
	std::unordered_map<std::string, std::string> IconMap{};

	std::string              GetIconFromBinaryName(std::string const& Binary);
	std::vector<std::string> DesktopDirs = {
		"/usr/share/applications",
	};

	using gtk_init_t = void (*)(int*, char***);
	using gtk_icon_theme_get_default_t = void* (*)();
	using gtk_icon_theme_load_icon_t = void* (*)(void*, char const*, int, int, void**);
	using gdk_pixbuf_get_width_t = int (*)(void*);
	using gdk_pixbuf_get_height_t = int (*)(void*);
	using gdk_pixbuf_get_n_channels_t = int (*)(void*);
	using gdk_pixbuf_get_rowstride_t = int (*)(void*);
	using gdk_pixbuf_get_pixels_t = unsigned char* (*)(void*);
	using g_object_unref_t = void (*)(void*);

	gtk_init_t                   gtk_init = nullptr;
	gtk_icon_theme_get_default_t gtk_icon_theme_get_default = nullptr;
	gtk_icon_theme_load_icon_t   gtk_icon_theme_load_icon = nullptr;
	gdk_pixbuf_get_width_t       gdk_pixbuf_get_width = nullptr;
	gdk_pixbuf_get_height_t      gdk_pixbuf_get_height = nullptr;
	gdk_pixbuf_get_n_channels_t  gdk_pixbuf_get_n_channels = nullptr;
	gdk_pixbuf_get_rowstride_t   gdk_pixbuf_get_rowstride = nullptr;
	gdk_pixbuf_get_pixels_t      gdk_pixbuf_get_pixels = nullptr;
	g_object_unref_t             g_object_unref = nullptr;

	void* GtkLibHandle = nullptr;
	void* GdkPixbufLibHandle = nullptr;
	void* GobjLibHandle = nullptr;
	void* GtkIconTheme = nullptr;

	bool bHaveGTK{};

public:
	void TryLoadGtkFunctions();

	void Cleanup();

	bool HasGtk() const { return bHaveGTK; }

	WIcon GtkLookupIcon(std::string const& IconName, int Size);

	std::string const&
	ResolveIcon(std::string const& Binary)
	{
		if (auto const Iter = IconMap.find(Binary); Iter != IconMap.end())
		{
			return Iter->second;
		}

		IconMap[Binary] = GetIconFromBinaryName(Binary);
		return IconMap[Binary];
	}
};
