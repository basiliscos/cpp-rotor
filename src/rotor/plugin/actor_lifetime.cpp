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

void actor_lifetime_plugin_t::deactivate() noexcept {
    auto& req = actor->shutdown_request;
    if (req) {
        actor->reply_to(*req);
        req.reset();
    }

    actor->get_state() = state_t::SHUTTED_DOWN;
    actor->shutdown_finish();
    plugin_t::deactivate();
}

void actor_lifetime_plugin_t::activate(actor_base_t* actor_) noexcept {
    actor = actor_;
    if (!actor_->address) {
        actor_->address = create_address();
    }
    plugin_t::activate(actor_);
}

address_ptr_t actor_lifetime_plugin_t::create_address() noexcept {
    return actor->get_supervisor().make_address();
}
