//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/initializer.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::internal;

void initializer_plugin_t::activate(actor_base_t *actor_) noexcept {
    actor = actor_;
    actor->install_plugin(*this, slot_t::INIT);
    actor->install_plugin(*this, slot_t::SUBSCRIPTION);
    actor->init_subscribe(*this);
    plugin_t::activate(actor);
}

processing_result_t initializer_plugin_t::handle_subscription(message::subscription_t& message) noexcept {
    tracked.remove(message.payload.point);
    if (tracked.empty()) {
        return processing_result_t::FINISHED;
    }
    return processing_result_t::IGNORED;
}

bool initializer_plugin_t::handle_init(message::init_request_t*) noexcept {
    return tracked.empty();
}
