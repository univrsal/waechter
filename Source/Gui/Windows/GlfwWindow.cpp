/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "GlfwWindow.hpp"

#include <curl/curl.h>

#define INCBIN_PREFIX G
#define STB_IMAGE_IMPLEMENTATION
#include "spdlog/spdlog.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "stb_image.h"
#include "incbin.h"
#include "implot.h"

#include "Filesystem.hpp"
#include "Time.hpp"
#include "AppIconAtlas.hpp"
#include "Client.hpp"
#include "Icons/IconAtlas.hpp"
#include "Util/I18n.hpp"
#include "Util/Settings.hpp"


INCBIN(Icon, ICON_PATH);
INCBIN(Font, FONT_PATH);

static void GlfwErrorCallback(int Error, char const* Description)
{
	spdlog::error("GLFW Error {}: {}", Error, Description);
}

bool WGlfwWindow::Init()
{
	curl_global_init(CURL_GLOBAL_DEFAULT);
	glfwSetErrorCallback(GlfwErrorCallback);
	MainWindow = std::make_unique<WMainWindow>();

	if (!glfwInit())
	{
		spdlog::critical("GLFW initialization failed!");
		return false;
	}
	char const* GlslVersion = "#version 130";
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
	glfwMakeContextCurrent(Window);

	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
	{
		glfwDestroyWindow(Window);
		glfwTerminate();
		spdlog::critical("Failed to initialize GLAD!");
		return false;
	}

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
	auto     Path = WSettings::GetConfigFolder() / "imgui.ini";

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

	auto FontSize = std::round(16.0f * MainScale);
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
	Style.FontScaleDpi = MainScale;

	ImGui_ImplGlfw_InitForOpenGL(Window, true);
	ImGui_ImplOpenGL3_Init(GlslVersion);

	WTimerManager::GetInstance().Start(glfwGetTime());
	WIconAtlas::GetInstance().Load();
	WI18n::GetInstance().Init();
	WClient::GetInstance().Start();
	return true;
}

void WGlfwWindow::RunLoop()
{
	ImVec4 ClearColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	while (!glfwWindowShouldClose(Window))
	{
		glfwPollEvents();
		if (glfwGetWindowAttrib(Window, GLFW_ICONIFIED) != 0)
		{
			ImGui_ImplGlfw_Sleep(50);
			continue;
		}
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
		glClearColor(
			ClearColor.x * ClearColor.w, ClearColor.y * ClearColor.w, ClearColor.z * ClearColor.w, ClearColor.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(Window);
		WTimerManager::GetInstance().UpdateTimers(glfwGetTime());
	}
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
	curl_global_cleanup();
}

void WGlfwWindow::SetTitle(std::string const& Title) const
{
	glfwSetWindowTitle(Window, Title.c_str());
}
