//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/plugin.h"
#include "rotor/actor_base.h"

using namespace rotor;

plugin_t::~plugin_t() {}

void plugin_t::activate(actor_base_t* actor_) noexcept {
    actor = actor_;
    actor->commit_plugin_activation(*this, true);
}

bool plugin_t::is_complete_for(slot_t, const subscription_point_t&) noexcept {
    std::abort();
}

void plugin_t::deactivate() noexcept {
    own_subscriptions.clear();
    actor->commit_plugin_deactivation(*this);
    actor = nullptr;
}

bool plugin_t::handle_shutdown(message::shutdown_request_t&) noexcept {
    return true;
}

bool plugin_t::handle_init(message::init_request_t&) noexcept {
    return true;
}
