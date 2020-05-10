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
    virtual bool is_complete_for(slot_t slot, const subscription_point_t& point) noexcept override;
    virtual bool handle_init(message::init_request_t&) noexcept override;

    template<typename Handler> void subscribe_actor(Handler&& handler) noexcept;
    subscription_points_t tracked;
};


}
