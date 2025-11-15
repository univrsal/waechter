#include "LogWindow.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <spdlog/spdlog.h>

void WImGuiLogSink::sink_it_(spdlog::details::log_msg const& Message)
{
	spdlog::memory_buf_t Formatted;
	formatter_->format(Message, Formatted);
	if (Owner)
	{
		Owner->Append(Message.level, std::string(Formatted.data(), Formatted.size()));
	}
}

WLogWindow::~WLogWindow()
{
	DetachFromSpdlog();
}

void WLogWindow::AttachToSpdlog()
{
	if (Sink)
		return;
	Sink = std::make_shared<WImGuiLogSink>(this);
	// Keep global logger config, just add our sink in addition to existing ones
	spdlog::default_logger()->sinks().push_back(Sink);
	spdlog::info("Wächter started");
}

void WLogWindow::DetachFromSpdlog()
{
	if (!Sink)
		return;
	auto& sinks = spdlog::default_logger()->sinks();
	sinks.erase(std::remove(sinks.begin(), sinks.end(), Sink), sinks.end());
	Sink.reset();
}

void WLogWindow::Append(spdlog::level::level_enum Level, std::string Msg)
{
	std::lock_guard Lock(Mutex);
	Entries.push_back(WLogEntry{ Level, std::move(Msg) });
	if (Entries.size() > MaxEntries)
	{
		Entries.pop_front();
	}

	if (AutoScroll)
	{
		ScrollToBottom = true;
	}
}

void WLogWindow::Clear()
{
	std::lock_guard Lock(Mutex);
	Entries.clear();
}

void WLogWindow::DoScrollToBottom()
{
	if (ScrollToBottom)
	{
		ImGui::SetScrollHereY(1.0f);
		ScrollToBottom = false;
	}
}

void WLogWindow::Draw()
{
	if (ImGui::Begin("Log", nullptr, ImGuiWindowFlags_NoCollapse))
	{
		ImGui::Checkbox("Auto-scroll", &AutoScroll);
		ImGui::SameLine();
		if (ImGui::Button("Clear"))
		{
			Clear();
		}

		ImGui::Separator();
		ImGui::BeginChild("LogRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

		{

			std::lock_guard<std::mutex> lk(Mutex);
			for (auto const& e : Entries)
			{
				ImVec4 col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
				switch (e.Level)
				{
					case spdlog::level::trace:
						col = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
						break;
					case spdlog::level::debug:
						col = ImVec4(0.5f, 0.7f, 1.0f, 1.0f);
						break;
					case spdlog::level::info:
						col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
						break;
					case spdlog::level::warn:
						col = ImVec4(1.0f, 0.7f, 0.2f, 1.0f);
						break;
					case spdlog::level::err:
					case spdlog::level::critical:
						col = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
						break;
					default:
						break;
				}

				ImGui::PushStyleColor(ImGuiCol_Text, col);
				ImGui::TextUnformatted(e.Text.c_str());
				ImGui::PopStyleColor();
			}
		}

		DoScrollToBottom();
		ImGui::EndChild();
	}
	ImGui::End();
}