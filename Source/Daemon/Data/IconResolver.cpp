//
// Created by usr on 02/11/2025.
//

#include "IconResolver.hpp"

#include <fstream>
#include <cstdlib>
#include <vector>

#include "Filesystem.hpp"

#include <dlfcn.h>

inline std::string FindDesktopFile(std::string const& BinaryName, std::vector<std::string> const& DesktopDirs)
{
	auto BinaryNameLowercase = BinaryName;
	std::transform(BinaryNameLowercase.begin(), BinaryNameLowercase.end(), BinaryNameLowercase.begin(), ::tolower);

	for (auto const& Dir : DesktopDirs)
	{
		for (auto const& Entry : stdfs::recursive_directory_iterator(Dir))
		{
			if (Entry.path().extension() == ".desktop")
			{
				std::ifstream File(Entry.path());
				std::string   Line;
				while (std::getline(File, Line))
				{
					auto LineLowercase = Line;
					std::ranges::transform(LineLowercase, LineLowercase.begin(), ::tolower);
					if (LineLowercase.rfind("exec=", 0) == 0 && LineLowercase.find(BinaryNameLowercase) != std::string::npos)
					{
						spdlog::info("Found desktop file for binary '{}' at path '{}'", BinaryName, Entry.path().string());
						return Entry.path();
					}
				}
			}
		}
	}
	return "";
}

inline std::string GetIconName(std::string const& DesktopFilePath)
{
	std::ifstream File(DesktopFilePath);
	std::string   Line;
	while (std::getline(File, Line))
	{
		if (Line.rfind("Icon=", 0) == 0)
		{
			return Line.substr(5);
		}
	}
	return "";
}

inline std::string FindIconPathByName(std::string const& Name)
{
	// Iterate through all icon directories and look for {name}.png
	// If found, return the path to the icon
	for (auto const& Dir : stdfs::recursive_directory_iterator("/usr/share/icons/hicolor"))
	{
		if (Dir.path().extension() == ".png" && Dir.path().filename().string().find(Name) != std::string::npos)
		{
			return Dir.path();
		}
	}
	return "";
}

std::string WIconResolver::GetIconFromBinaryName(std::string const& Binary)
{
	auto const DesktopFile = FindDesktopFile(Binary, DesktopDirs);
	if (DesktopFile.empty())
	{
		spdlog::warn("No desktop file found for binary '{}'", Binary);
		return "";
	}

	auto const Name = GetIconName(DesktopFile);
	if (Name.empty())
	{
		spdlog::warn("No icon name found in desktop file '{}'", DesktopFile);
		return "";
	}
	return FindIconPathByName(Name);
}

void WIconResolver::TryLoadGtkFunctions()
{
	GtkLibHandle = dlopen("libgtk-3.so.0", RTLD_LAZY);
	if (!GtkLibHandle)
	{
		spdlog::info("GTK library not found, skipping GTK icon resolution");
		return; // GTK not available
	}

	GdkPixbufLibHandle = dlopen("libgdk_pixbuf-2.0.so.0", RTLD_LAZY);
	if (!GdkPixbufLibHandle)
	{
		spdlog::info("GDK Pixbuf library not found, skipping GTK icon resolution");
		dlclose(GtkLibHandle);
		GtkLibHandle = nullptr;
		return; // GDK Pixbuf not available
	}

	GobjLibHandle = dlopen("libgobject-2.0.so.0", RTLD_LAZY);
	if (!GobjLibHandle)
	{
		spdlog::info("libgobject-2.0.so.0 not found");
		dlclose(GdkPixbufLibHandle);
		dlclose(GtkLibHandle);
		GtkLibHandle = nullptr;
		GdkPixbufLibHandle = nullptr;
		return; // GObject not available
	}

	gtk_init = (gtk_init_t)dlsym(GtkLibHandle, "gtk_init");
	gtk_icon_theme_get_default = (gtk_icon_theme_get_default_t)dlsym(GtkLibHandle, "gtk_icon_theme_get_default");
	gtk_icon_theme_load_icon = (gtk_icon_theme_load_icon_t)dlsym(GtkLibHandle, "gtk_icon_theme_load_icon");

	gdk_pixbuf_get_width = (gdk_pixbuf_get_width_t)dlsym(GdkPixbufLibHandle, "gdk_pixbuf_get_width");
	gdk_pixbuf_get_height = (gdk_pixbuf_get_height_t)dlsym(GdkPixbufLibHandle, "gdk_pixbuf_get_height");
	gdk_pixbuf_get_n_channels = (gdk_pixbuf_get_n_channels_t)dlsym(GdkPixbufLibHandle, "gdk_pixbuf_get_n_channels");
	gdk_pixbuf_get_rowstride = (gdk_pixbuf_get_rowstride_t)dlsym(GdkPixbufLibHandle, "gdk_pixbuf_get_rowstride");
	gdk_pixbuf_get_pixels = (gdk_pixbuf_get_pixels_t)dlsym(GdkPixbufLibHandle, "gdk_pixbuf_get_pixels");

	g_object_unref = (g_object_unref_t)dlsym(GobjLibHandle, "g_object_unref");

	if (!gtk_init || !gtk_icon_theme_get_default || !gtk_icon_theme_load_icon || !gdk_pixbuf_get_width || !gdk_pixbuf_get_height || !gdk_pixbuf_get_pixels)
	{
		spdlog::info("Failed to load GTK functions, skipping GTK icon resolution");
		Cleanup();
		return; // Missing function = no GTK runtime → fallback
	}

	gtk_init(nullptr, nullptr);
	GtkIconTheme = gtk_icon_theme_get_default();
	spdlog::info("GTK functions loaded successfully for icon resolution");
	bHaveGTK = true;
}

void WIconResolver::Cleanup()
{
	dlclose(GtkLibHandle);
	dlclose(GdkPixbufLibHandle);
	dlclose(GobjLibHandle);
}

#define GKT_ICON_LOOKUP_USE_BUILTIN 4
#define GTK_ICON_LOOKUP_GENERIC_FALLBACK 8
#define GTK_ICON_LOOKUP_FORCE_SIZE 16

WIcon WIconResolver::GtkLookupIcon(std::string const& IconName, int Size)
{
	void* Pixbuf = gtk_icon_theme_load_icon(GtkIconTheme, IconName.c_str(), Size, GTK_ICON_LOOKUP_FORCE_SIZE | GKT_ICON_LOOKUP_USE_BUILTIN | GTK_ICON_LOOKUP_GENERIC_FALLBACK, nullptr);
	if (!Pixbuf)
	{
		return {}; // No icon → fallback
	}

	WIcon Icon;
	Icon.Width = static_cast<uint>(gdk_pixbuf_get_width(Pixbuf));
	Icon.Height = static_cast<uint>(gdk_pixbuf_get_height(Pixbuf));

	uint           Channels = static_cast<uint>(gdk_pixbuf_get_n_channels(Pixbuf));
	uint           Stride = static_cast<uint>(gdk_pixbuf_get_rowstride(Pixbuf));
	unsigned char* Pixels = gdk_pixbuf_get_pixels(Pixbuf);

	Icon.Rgba.resize(Icon.Width * Icon.Height * 4);

	for (uint Y = 0; Y < Icon.Height; Y++)
		for (uint X = 0; X < Icon.Width; X++)
		{
			unsigned char* P = Pixels + Y * Stride + X * Channels;
			uint           Idx = (Y * Icon.Width + X) * 4;
			Icon.Rgba[Idx + 0] = P[0];
			Icon.Rgba[Idx + 1] = P[1];
			Icon.Rgba[Idx + 2] = P[2];
			Icon.Rgba[Idx + 3] = (Channels == 4 ? P[3] : 255);
		}

	g_object_unref(Pixbuf);
	return Icon;
}