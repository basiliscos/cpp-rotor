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


void plugin_t::deactivate() noexcept {
    if (actor) {
        own_subscriptions.clear();
        actor->commit_plugin_deactivation(*this);
        actor = nullptr;
    }
}

bool plugin_t::handle_shutdown(message::shutdown_request_t*) noexcept {
    return true;
}

bool plugin_t::handle_init(message::init_request_t*) noexcept {
    return true;
}

processing_result_t plugin_t::handle_subscription(message::subscription_t&) noexcept {
    std::abort();
}

processing_result_t plugin_t::handle_unsubscription(message::unsubscription_t&) noexcept {
    std::abort();
}

processing_result_t plugin_t::handle_unsubscription_external(message::unsubscription_external_t&) noexcept {
    std::abort();
}

