/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <mutex>
#include <string>
#include <memory>
#include <unordered_map>

#include "IPAddress.hpp"
#include "IP2Asn/IP2AsnDB.hpp"

class WTrafficTree;

class WBuffer;

class WDetailsWindow
{
	std::mutex                    DataMutex;
	std::shared_ptr<WTrafficTree> Tree{};

	void DrawFilterDetails() const;
	void DrawSystemDetails() const;
	void DrawApplicationDetails() const;
	void DrawProcessDetails() const;
	void DrawSocketDetails() const;
	void DrawTupleDetails() const;

	std::string                                         FormattedUptime{};
	std::unordered_map<WIPAddress, WIP2AsnLookupResult> LookupCache{};

public:
	explicit WDetailsWindow();
	void Draw();

	void HandleLookupResult(WBuffer const& Buffer);
};
