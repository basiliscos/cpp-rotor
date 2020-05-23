#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin.h"

namespace rotor::internal {

struct starter_plugin_t: public plugin_t {

    static const void* class_identity;
    const void* identity() const noexcept override;

    void activate(actor_base_t* actor) noexcept override;
    void deactivate() noexcept override;

    void on_start(message::start_trigger_t& message) noexcept;
    virtual bool handle_init(message::init_request_t*) noexcept override;

    processing_result_t handle_subscription(message::subscription_t& message) noexcept override;

    template<typename Handler> void subscribe_actor(Handler&& handler) noexcept;
    template<typename Handler> void subscribe_actor(Handler&& handler, const address_ptr_t& addr) noexcept;


    subscription_points_t tracked;
};


}
