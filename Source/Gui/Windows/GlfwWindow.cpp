//
// Created by usr on 26/10/2025.
//

#include "GlfwWindow.hpp"
#define STB_IMAGE_IMPLEMENTATION

#include <spdlog/spdlog.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <stb_image.h>
#include <incbin.h>
#include <implot.h>

#include "Filesystem.hpp"
#include "Time.hpp"
#include "AppIconAtlas.hpp"

INCBIN(Icon, ICON_PATH);
INCBIN(Font, FONT_PATH);

static void GlfwErrorCallback(int Error, char const* Description)
{
	spdlog::error("GLFW Error {}: {}", Error, Description);
}

static stdfs::path GetConfigFolder()
{
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

bool WGlfwWindow::Init()
{
	glfwSetErrorCallback(GlfwErrorCallback);
	MainWindow = std::make_unique<WMainWindow>();

	if (!glfwInit())
	{
		spdlog::critical("GLFW initialization failed!");
		return false;
	}
	char const* GlslVersion = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	MainScale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor()); // Valid on GLFW 3.3+ only

	Window = glfwCreateWindow(900, 700, "Wächter", nullptr, nullptr);
	if (!Window)
	{
		spdlog::critical("GLFW window creation failed!");
		glfwTerminate();
		return false;
	}
	glfwMakeContextCurrent(Window);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		glfwDestroyWindow(Window);
		glfwTerminate();
		spdlog::critical("Failed to initialize GLAD!");
		return false;
	}

	int            Width, Height, Channels;
	unsigned char* Pixels =
		stbi_load_from_memory(gIconData, static_cast<int>(gIconSize), &Width, &Height, &Channels, 4);
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
	auto     Path = GetConfigFolder() / "imgui.ini";

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

	auto  FontSize = std::round(16.0f * MainScale);
	auto* FontData = Io.Fonts->AddFontFromMemoryTTF((void*)gFontData, static_cast<int>(gFontSize), FontSize, &Cfg);
	Io.FontDefault = FontData;
	// Setup Dear ImGui style
	ImGui::StyleColorsLight();
	ImGuiStyle& Style = ImGui::GetStyle();
	Style.ScaleAllSizes(MainScale);
	Style.FontScaleDpi = MainScale;

	ImGui_ImplGlfw_InitForOpenGL(Window, true);
	ImGui_ImplOpenGL3_Init(GlslVersion);

	WTimerManager::GetInstance().Start(glfwGetTime());
	WAppIconAtlas::GetInstance().Init();
	MainWindow->GetClient().Start();
	return true;
}

void WGlfwWindow::RunLoop()
{
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	while (!glfwWindowShouldClose(Window))
	{
		glfwPollEvents();
		if (glfwGetWindowAttrib(Window, GLFW_ICONIFIED) != 0)
		{
			ImGui_ImplGlfw_Sleep(10);
			continue;
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
			clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(Window);
		WTimerManager::GetInstance().UpdateTimers(glfwGetTime());
	}
	MainWindow = nullptr;
}

void WGlfwWindow::Destroy()
{
	MainWindow = nullptr;
	WAppIconAtlas::GetInstance().Cleanup();
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
}
