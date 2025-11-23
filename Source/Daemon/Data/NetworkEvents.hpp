//
// Created by usr on 22/11/2025.
//

#pragma once
#include <sigslot/signal.hpp>

#include "Singleton.hpp"

struct WSocketCounter;
struct WProcessCounter;

class WNetworkEvents final : public TSingleton<WNetworkEvents>
{
public:
	sigslot::signal<std::shared_ptr<WSocketCounter>>         OnSocketCreated;
	sigslot::signal<std::shared_ptr<WSocketCounter> const&>  OnSocketRemoved;
	sigslot::signal<std::shared_ptr<WProcessCounter> const&> OnProcessRemoved;
};