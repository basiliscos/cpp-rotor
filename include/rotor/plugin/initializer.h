#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin.h"

namespace rotor::internal {

struct initializer_plugin_t: public plugin_t {
    virtual void activate(actor_base_t* actor) noexcept override;
    virtual bool handle_init(message::init_request_t*) noexcept override;

    processing_result_t handle_subscription(message::subscription_t& message) noexcept override;

    template<typename Handler> void subscribe_actor(Handler&& handler) noexcept;
    subscription_points_t tracked;
};


}
