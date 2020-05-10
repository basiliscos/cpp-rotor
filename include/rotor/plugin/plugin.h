#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "../subscription_point.h"
#include "rotor/messages.hpp"

namespace rotor {

enum class slot_t { INIT = 0, SHUTDOWN, SUBSCRIPTION, UNSUBSCRIPTION };

struct plugin_t {

    plugin_t() = default;
    virtual ~plugin_t();

    virtual bool is_complete_for(slot_t slot, const subscription_point_t& point) noexcept;
    virtual void activate(actor_base_t* actor) noexcept;
    virtual void deactivate() noexcept;

    virtual bool handle_init(message::init_request_t* message) noexcept;
    virtual bool handle_shutdown(message::shutdown_request_t* message) noexcept;

    template<typename Handler> handler_ptr_t subscribe(Handler&& handler, const address_ptr_t& address) noexcept;
    template<typename Handler> handler_ptr_t subscribe(Handler&& handler) noexcept;

    actor_base_t* actor;
    subscription_points_t own_subscriptions;
};

}
