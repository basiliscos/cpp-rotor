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
enum class processing_result_t { CONSUMED = 0, IGNORED, FINISHED };

struct plugin_t {

    plugin_t() = default;
    virtual ~plugin_t();

    virtual const void* identity() const noexcept = 0;

    virtual bool activate(actor_base_t* actor) noexcept;
    virtual bool deactivate() noexcept;

    virtual bool handle_init(message::init_request_t* message) noexcept;
    virtual bool handle_shutdown(message::shutdown_request_t* message) noexcept;

    virtual processing_result_t handle_subscription(message::subscription_t& message) noexcept;
    virtual processing_result_t handle_unsubscription(message::unsubscription_t& message) noexcept;
    virtual processing_result_t handle_unsubscription_external(message::unsubscription_external_t& message) noexcept;

    template<typename Handler> handler_ptr_t subscribe(Handler&& handler, const address_ptr_t& address) noexcept;
    template<typename Handler> handler_ptr_t subscribe(Handler&& handler) noexcept;

    actor_base_t* actor;
    subscription_points_t own_subscriptions;
};

}
