/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "SdlWindow.hpp"

#ifdef __EMSCRIPTEN__
	#include <emscripten.h>
	#include <emscripten/html5.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "spdlog/spdlog.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "stb_image.h"
#include "implot.h"

#include "Assets.hpp"
#include "Filesystem.hpp"
#include "Time.hpp"
#include "AppIconAtlas.hpp"
#include "Client.hpp"
#include "Icons/IconAtlas.hpp"
#include "Util/I18n.hpp"
#include "Util/LibCurl.hpp"
#include "Util/Settings.hpp"
#include "Util/SysUtil.hpp"
#include "Util/TrayIcon.hpp"

void WSdlWindow::Tick()
{
	static constexpr ImVec4 ClearColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	SDL_Event Event;
	while (SDL_PollEvent(&Event))
	{
		ImGui_ImplSDL2_ProcessEvent(&Event);

		if (Event.type == SDL_QUIT)
		{
#ifndef __EMSCRIPTEN__
			if (WSettings::GetInstance().bEnableTrayIcon && WSettings::GetInstance().bMinimizeToTrayOnClose)
			{
				SDL_HideWindow(Window);
			}
			else
#endif
			{
				bShouldClose = true;
			}
		}

		if (Event.type == SDL_WINDOWEVENT)
		{
			if (Event.window.event == SDL_WINDOWEVENT_CLOSE && Event.window.windowID == SDL_GetWindowID(Window))
			{
#ifndef __EMSCRIPTEN__
				if (WSettings::GetInstance().bEnableTrayIcon && WSettings::GetInstance().bMinimizeToTrayOnClose)
				{
					SDL_HideWindow(Window);
				}
				else
#endif
				{
					bShouldClose = true;
				}
			}
		}
	}

#ifndef __EMSCRIPTEN__
	WTrayIcon::GetInstance().Step();
#endif

	if (SDL_GetWindowFlags(Window) & SDL_WINDOW_MINIMIZED)
	{
		SDL_Delay(50);
		return;
	}

#ifdef __EMSCRIPTEN__
	// Update canvas size to match CSS dimensions and device pixel ratio for HiDPI
	double CanvasWidth, CanvasHeight;
	emscripten_get_element_css_size("#canvas", &CanvasWidth, &CanvasHeight);
	double DevicePixelRatio = emscripten_get_device_pixel_ratio();
	int    ActualWidth = static_cast<int>(CanvasWidth * DevicePixelRatio);
	int    ActualHeight = static_cast<int>(CanvasHeight * DevicePixelRatio);

	int CurrentWidth, CurrentHeight;
	SDL_GetWindowSize(Window, &CurrentWidth, &CurrentHeight);
	if (CurrentWidth != ActualWidth || CurrentHeight != ActualHeight)
	{
		SDL_SetWindowSize(Window, ActualWidth, ActualHeight);
	}
	// force one initial scale update
	float NewScale = static_cast<float>(DevicePixelRatio);
	if (std::abs(NewScale - MainScale) > 0.01f)
	{
		spdlog::info("Content scale changed from {:.2f} to {:.2f}", MainScale, NewScale);
		WSysUtil::UpdateImGuiScale(MainScale, NewScale);
		MainScale = NewScale;
	}
#else
	// Check for DPI scale changes on desktop (e.g., moving the window between monitors
	// with different scale factors, or Wayland compositor scale changes).
	// Compute the ratio of drawable (physical) pixels to logical pixels; this is the
	// only reliable way to get the true scale on Wayland with SDL2.
	{
		auto  Index = SDL_GetWindowDisplayIndex(Window);
		float Ddpi, Hdpi, Vdpi;
		auto  NewScale = SDL_GetDisplayDPI(Index, &Ddpi, &Hdpi, &Vdpi) == 0 ? Ddpi / 96.0f : 1.0f;
		if (NewScale >= 0.5f && std::abs(NewScale - MainScale) > 0.01f)
		{
			spdlog::info("Content scale changed from {:.2f} to {:.2f}", MainScale, NewScale);
			WSysUtil::UpdateImGuiScale(MainScale, NewScale);
			MainScale = NewScale;
		}
	}
#endif

	if (WSettings::GetInstance().bReduceFrameRateWhenInactive && !(SDL_GetWindowFlags(Window) & SDL_WINDOW_INPUT_FOCUS))
	{
		SDL_Delay(10);
	}

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	// Perform any pending GL uploads (e.g., icon atlas) on the render thread
	WAppIconAtlas::GetInstance().UploadPendingIfAny();

	MainWindow->Draw();

	// Rendering
	ImGui::Render();
	int display_w, display_h;
	SDL_GL_GetDrawableSize(Window, &display_w, &display_h);
	glViewport(0, 0, display_w, display_h);
	glClearColor(ClearColor.x * ClearColor.w, ClearColor.y * ClearColor.w, ClearColor.z * ClearColor.w, ClearColor.w);
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	SDL_GL_SwapWindow(Window);
	WTimerManager::GetInstance().UpdateTimers(static_cast<double>(SDL_GetTicks()) / 1000.0);
}

bool WSdlWindow::Init()
{
#if __EMSCRIPTEN__
	// Get the device pixel ratio to properly set canvas resolution for HiDPI displays
	double DevicePixelRatio = emscripten_get_device_pixel_ratio();
	double CanvasWidth, CanvasHeight;
	emscripten_get_element_css_size("#canvas", &CanvasWidth, &CanvasHeight);
#else
	WLibCurl::Init();
#endif

	MainWindow = std::make_unique<WMainWindow>();

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
	{
		spdlog::critical("SDL initialization failed: {}", SDL_GetError());
		return false;
	}

#if __EMSCRIPTEN__
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#endif
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	// Hint for IME support
	SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

#if __EMSCRIPTEN__
	MainScale = static_cast<float>(DevicePixelRatio);
#endif

	Uint32 WindowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
	Window = SDL_CreateWindow("Wächter", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 900, 700, WindowFlags);
	if (!Window)
	{
		spdlog::critical("SDL window creation failed: {}", SDL_GetError());
		SDL_Quit();
		return false;
	}

	GlContext = SDL_GL_CreateContext(Window);
	if (!GlContext)
	{
		spdlog::critical("SDL GL context creation failed: {}", SDL_GetError());
		SDL_DestroyWindow(Window);
		SDL_Quit();
		return false;
	}
	SDL_GL_MakeCurrent(Window, GlContext);
	SDL_GL_SetSwapInterval(1); // vsync

#if !defined(__EMSCRIPTEN__)
	// Compute the true DPI scale from the ratio of drawable (physical) pixels to logical pixels.
	// SDL_GetDisplayDPI returns 1.0f on Wayland (the compositor manages scaling), so we derive
	// the scale from SDL_GL_GetDrawableSize vs SDL_GetWindowSize instead, which works correctly
	// across X11, Wayland, and other platforms.
	{
		int DrawableW = 0, LogicalW = 0;
		SDL_GL_GetDrawableSize(Window, &DrawableW, nullptr);
		SDL_GetWindowSize(Window, &LogicalW, nullptr);
		if (LogicalW > 0 && DrawableW > 0)
		{
			float ComputedScale = static_cast<float>(DrawableW) / static_cast<float>(LogicalW);
			if (ComputedScale >= 0.5f)
				MainScale = ComputedScale;
		}
		// Fall back to the SDL DPI helper if the drawable/logical ratio is 1:1
		// (e.g., on X11 without high-DPI framebuffer support).
		if (MainScale == 1.0f)
			MainScale = ImGui_ImplSDL2_GetContentScaleForDisplay(SDL_GetWindowDisplayIndex(Window));
		spdlog::info("Detected DPI scale: {:.2f} (driver: {})", MainScale,
			SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "unknown");
	}
#endif

#if !defined(__EMSCRIPTEN__)
	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress)))
	{
		SDL_GL_DeleteContext(GlContext);
		SDL_DestroyWindow(Window);
		SDL_Quit();
		spdlog::critical("Failed to initialize GLAD!");
		return false;
	}
#endif

	// Set window icon
	int            Width, Height, Channels;
	unsigned char* Pixels =
		stbi_load_from_memory(GIconData, static_cast<int>(GIconSize), &Width, &Height, &Channels, 4);
	if (Pixels)
	{
		SDL_Surface* IconSurface = SDL_CreateRGBSurfaceFrom(Pixels, Width, Height, 32, Width * 4,
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
			0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF
#else
			0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
#endif
		);
		if (IconSurface)
		{
			SDL_SetWindowIcon(Window, IconSurface);
			SDL_FreeSurface(IconSurface);
		}
		stbi_image_free(Pixels);
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();
	ImGuiIO& Io = ImGui::GetIO();
	auto     Path = WSysUtil::GetConfigFolder() / "imgui.ini";

	if (!Path.empty())
	{
		ImGuiIniPath = Path.string();
		Io.IniFilename = ImGuiIniPath.c_str();
	}
	else
	{
		Io.IniFilename = nullptr; // disable auto-load/save
	}

	Io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
	Io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
	Io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking

	ImFontConfig Cfg;
	Cfg.OversampleH = 2;
	Cfg.OversampleV = 2;
	Cfg.PixelSnapH = true;
	Cfg.FontDataOwnedByAtlas = false; // font data comes from static incbin; do not let ImGui free it

	auto FontSize = std::round(14.0f * MainScale);
	auto NonConstFontData = const_cast<unsigned char*>(GFontData);

	auto* FontData = Io.Fonts->AddFontFromMemoryTTF(NonConstFontData, static_cast<int>(GFontSize), FontSize, &Cfg);
	Io.FontDefault = FontData;

	// Setup Dear ImGui style
	if (WSettings::GetInstance().bUseDarkTheme)
	{
		ImGui::StyleColorsDark();
	}
	else
	{
		ImGui::StyleColorsLight();
	}
	ImGuiStyle& Style = ImGui::GetStyle();
	Style.ScaleAllSizes(MainScale);

	ImGui_ImplSDL2_InitForOpenGL(Window, GlContext);
	ImGui_ImplOpenGL3_Init(GetPreferredShaderVersion());

	WTimerManager::GetInstance().Start(static_cast<double>(SDL_GetTicks()) / 1000.0);
	WIconAtlas::GetInstance().Load();
	WI18n::GetInstance().Init();
#if __EMSCRIPTEN__
	// don't auto-connect to the placeholder server
	if (WSettings::GetInstance().SocketPath != "wss://example.com/ws")
#endif
		WClient::GetInstance().Start();

#ifndef __EMSCRIPTEN__
	WTrayIcon::GetInstance().Init();
#endif
	return true;
}

static void Tick(void* Arg)
{
	auto* Self = static_cast<WSdlWindow*>(Arg);
	Self->Tick();
}

void WSdlWindow::RunLoop()
{

#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop_arg(::Tick, this, 0, true);
#else
	while (!bShouldClose)
	{
		Tick();
	}
#endif

	WClient::GetInstance().Stop();
	MainWindow = nullptr;
}

void WSdlWindow::Destroy()
{
	MainWindow = nullptr;
	WSettings::GetInstance().Save();
#ifndef __EMSCRIPTEN__
	WTrayIcon::GetInstance().Destroy();
#endif
	WAppIconAtlas::GetInstance().Cleanup();
	WIconAtlas::GetInstance().Unload();
	if (ImGui::GetCurrentContext())
	{
		ImGuiIO& io = ImGui::GetIO();
		if (io.IniFilename && io.IniFilename[0] != '\0')
		{
			ImGui::SaveIniSettingsToDisk(io.IniFilename);
		}
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImPlot::DestroyContext();
	ImGui::DestroyContext();

	SDL_GL_DeleteContext(GlContext);
	SDL_DestroyWindow(Window);
	SDL_Quit();
	WSysUtil::SyncFilesystemToIndexedDB();
#ifndef __EMSCRIPTEN__
	WLibCurl::Deinit();
#endif
}

void WSdlWindow::SetTitle(std::string const& Title) const
{
	SDL_SetWindowTitle(Window, Title.c_str());
}
