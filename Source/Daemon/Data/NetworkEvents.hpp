/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "sigslot/signal.hpp"

#include "Singleton.hpp"
#include "IPAddress.hpp"

struct WSocketCounter;
struct WProcessCounter;
struct WTupleCounter;

class WNetworkEvents final : public TSingleton<WNetworkEvents>
{
public:
	sigslot::signal<WSocketCounter const*>                                   OnSocketConnected;
	sigslot::signal<std::shared_ptr<WTupleCounter> const&, WEndpoint const&> OnUDPTupleCreated;
	sigslot::signal<std::shared_ptr<WSocketCounter> const&>                  OnSocketRemoved;
	sigslot::signal<std::shared_ptr<WProcessCounter> const&>                 OnProcessRemoved;
	sigslot::signal<std::shared_ptr<WProcessCounter> const&>                 OnProcessCreated;
};