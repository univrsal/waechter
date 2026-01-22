/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <deque>
#include <memory>
#include <mutex>
#include <string>

#include "spdlog/sinks/base_sink.h"

struct WLogEntry
{
	spdlog::level::level_enum Level;
	std::string               Text;
};

class WLogWindow;

class WImGuiLogSink : public spdlog::sinks::base_sink<std::mutex>
{
	WLogWindow* Owner{};

public:
	explicit WImGuiLogSink(WLogWindow* InOwner) : Owner(InOwner) {}

protected:
	void sink_it_(spdlog::details::log_msg const& Message) override;
	void flush_() override {}
};

class WLogWindow
{
	std::mutex                     Mutex;
	bool                           ScrollToBottom{ true };
	bool                           AutoScroll{ true };
	static constexpr size_t        MaxEntries{ 1000 };
	std::deque<WLogEntry>          Entries{};
	std::shared_ptr<WImGuiLogSink> Sink{};

public:
	~WLogWindow();

	void AttachToSpdlog();
	void DetachFromSpdlog();
	void Append(spdlog::level::level_enum Level, std::string Msg);
	void Clear();
	void DoScrollToBottom();
	void Draw();
};