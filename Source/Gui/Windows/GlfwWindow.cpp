/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "GlfwWindow.hpp"

#ifdef __EMSCRIPTEN__
	#include <GLFW/glfw3.h>
	#include <emscripten.h>
	#include <emscripten/html5.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "spdlog/spdlog.h"
#include "imgui_impl_glfw.h"
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

static void GlfwErrorCallback(int Error, char const* Description)
{
	spdlog::error("GLFW Error {}: {}", Error, Description);
}

void WGlfwWindow::ContentScaleCallback(GLFWwindow* Window, float XScale, float YScale)
{
	// Use the larger of the two scales (they should typically be the same)
	float NewScale = std::max(XScale, YScale);
	
	// Get the WGlfwWindow instance from the window user pointer
	auto* Self = static_cast<WGlfwWindow*>(glfwGetWindowUserPointer(Window));
	if (Self && std::abs(NewScale - Self->MainScale) > 0.01f)
	{
		spdlog::info("Content scale changed from {:.2f} to {:.2f}", Self->MainScale, NewScale);
		// Update ImGui scaling (handles both style and font scaling)
		WSysUtil::UpdateImGuiScale(Self->MainScale, NewScale);
		Self->MainScale = NewScale;
	}
}

void WGlfwWindow::Tick() const
{
	static constexpr ImVec4 ClearColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	glfwPollEvents();
	if (glfwGetWindowAttrib(Window, GLFW_ICONIFIED) != 0)
	{
		ImGui_ImplGlfw_Sleep(50);
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
	emscripten_get_canvas_element_size("#canvas", &CurrentWidth, &CurrentHeight);
	if (CurrentWidth != ActualWidth || CurrentHeight != ActualHeight)
	{
		emscripten_set_canvas_element_size("#canvas", ActualWidth, ActualHeight);
		glfwSetWindowSize(Window, ActualWidth, ActualHeight);
	}
	// force one initial update
	ContentScaleCallback(Window, DevicePixelRatio, DevicePixelRatio);
#endif
	if (WSettings::GetInstance().bReduceFrameRateWhenInactive && glfwGetWindowAttrib(Window, GLFW_FOCUSED) != 1)
	{
		ImGui_ImplGlfw_Sleep(10);
	}

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// Perform any pending GL uploads (e.g., icon atlas) on the render thread
	WAppIconAtlas::GetInstance().UploadPendingIfAny();

	MainWindow->Draw();

	// Rendering
	ImGui::Render();
	int display_w, display_h;
	glfwGetFramebufferSize(Window, &display_w, &display_h);
	glViewport(0, 0, display_w, display_h);
	glClearColor(ClearColor.x * ClearColor.w, ClearColor.y * ClearColor.w, ClearColor.z * ClearColor.w, ClearColor.w);
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	glfwSwapBuffers(Window);
	WTimerManager::GetInstance().UpdateTimers(glfwGetTime());
}

bool WGlfwWindow::Init()
{
#if __EMSCRIPTEN__
	// Get the device pixel ratio to properly set canvas resolution for HiDPI displays
	double DevicePixelRatio = emscripten_get_device_pixel_ratio();
	double CanvasWidth, CanvasHeight;
	emscripten_get_element_css_size("#canvas", &CanvasWidth, &CanvasHeight);
	emscripten_set_canvas_element_size(
		"#canvas", static_cast<int>(CanvasWidth * DevicePixelRatio), static_cast<int>(CanvasHeight * DevicePixelRatio));
#else
	WLibCurl::Init();
#endif

	glfwSetErrorCallback(GlfwErrorCallback);
	MainWindow = std::make_unique<WMainWindow>();

	if (!glfwInit())
	{
		spdlog::critical("GLFW initialization failed!");
		return false;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	MainScale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor()); // Valid on GLFW 3.3+ only


	Window = glfwCreateWindow(900, 700, "Wächter", nullptr, nullptr);
	if (!Window)
	{
		spdlog::critical("GLFW window creation failed!");
		glfwTerminate();
		return false;
	}
	
	// Set window user pointer to this instance so we can access it in callbacks
	glfwSetWindowUserPointer(Window, this);
	
	// Register content scale callback to handle DPI changes when moving between monitors
	glfwSetWindowContentScaleCallback(Window, ContentScaleCallback);

	glfwMakeContextCurrent(Window);
#if !defined(__EMSCRIPTEN__)
	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
	{
		glfwDestroyWindow(Window);
		glfwTerminate();
		spdlog::critical("Failed to initialize GLAD!");
		return false;
	}
#endif

	int            Width, Height, Channels;
	unsigned char* Pixels =
		stbi_load_from_memory(GIconData, static_cast<int>(GIconSize), &Width, &Height, &Channels, 4);
	if (Pixels)
	{
		GLFWimage Image{};
		Image.width = Width;
		Image.height = Height;
		Image.pixels = Pixels;
		glfwSetWindowIcon(Window, 1, &Image);
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

	ImGui_ImplGlfw_InitForOpenGL(Window, true);
	ImGui_ImplOpenGL3_Init(GetPreferredShaderVersion());

	WTimerManager::GetInstance().Start(glfwGetTime());
	WIconAtlas::GetInstance().Load();
	WI18n::GetInstance().Init();
#if EMSCRIPTEN
	// don't auto-connect to the placeholder server
	if (WSettings::GetInstance().SocketPath != "wss://example.com/ws")
#endif
		WClient::GetInstance().Start();
	return true;
}

static void Tick(void* Arg)
{
	auto* Self = static_cast<WGlfwWindow*>(Arg);
	Self->Tick();
}

void WGlfwWindow::RunLoop()
{

#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop_arg(::Tick, this, 0, true);
#else
	while (!glfwWindowShouldClose(Window))
	{
		Tick();
	}
#endif

	WClient::GetInstance().Stop();
	MainWindow = nullptr;
}

void WGlfwWindow::Destroy()
{
	MainWindow = nullptr;
	WSettings::GetInstance().Save();
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
	ImGui_ImplGlfw_Shutdown();
	ImPlot::DestroyContext();
	ImGui::DestroyContext();

	glfwDestroyWindow(Window);
	glfwTerminate();
	WSysUtil::SyncFilesystemToIndexedDB();
#ifndef __EMSCRIPTEN__
	WLibCurl::Deinit();
#endif
}

void WGlfwWindow::SetTitle(std::string const& Title) const
{
	glfwSetWindowTitle(Window, Title.c_str());
}
