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

INCBIN(Icon, ICON_PATH);
INCBIN(Font, FONT_PATH);

static void GlfwErrorCallback(int error, const char* description)
{
	spdlog::error("GLFW Error {}: {}", error, description);
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
	const char* glsl_version = "#version 130";
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
	unsigned char* Pixels = stbi_load_from_memory(gIconData, gIconSize, &Width, &Height, &Channels, 4);
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
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

	ImFontConfig cfg;
	cfg.OversampleH = 2;
	cfg.OversampleV = 2;
	cfg.PixelSnapH = true;

	auto  fontSize = std::round(16.0f * MainScale);
	auto* FontData = io.Fonts->AddFontFromMemoryTTF((void*)gFontData, gFontSize, fontSize, &cfg);
	io.FontDefault = FontData;
	// Setup Dear ImGui style
	ImGui::StyleColorsLight();
	ImGuiStyle& style = ImGui::GetStyle();
	style.ScaleAllSizes(MainScale);
	style.FontScaleDpi = MainScale;

	ImGui_ImplGlfw_InitForOpenGL(Window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);
	return true;
}

void WGlfwWindow::RunLoop()
{
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	ImGuiIO& io = ImGui::GetIO();
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

		MainWindow->Draw();

		// Rendering
		ImGui::Render();
		int display_w, display_h;
		glfwGetFramebufferSize(Window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(Window);
	}
	MainWindow = nullptr;
}

WGlfwWindow::~WGlfwWindow()
{
	glfwDestroyWindow(Window);
	glfwTerminate();
}
