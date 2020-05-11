//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/actor_lifetime.h"
#include "rotor/actor_base.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::internal;

bool actor_lifetime_plugin_t::activate(actor_base_t* actor_) noexcept {
    actor = actor_;
    actor->state = state_t::INITIALIZING;

    if (!actor_->address) {
        actor_->address = create_address();
    }
    return plugin_t::activate(actor_);
}


bool actor_lifetime_plugin_t::deactivate() noexcept {
    auto& req = actor->shutdown_request;
    if (req) {
        actor->reply_to(*req);
        req.reset();
    }

    actor->get_state() = state_t::SHUTTED_DOWN;
    actor->shutdown_finish();
    return plugin_t::deactivate();
}

address_ptr_t actor_lifetime_plugin_t::create_address() noexcept {
    return actor->get_supervisor().make_address();
}
