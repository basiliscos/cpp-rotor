//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/initializer.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::internal;

const void* initializer_plugin_t::class_identity = static_cast<const void *>(typeid(initializer_plugin_t).name());

const void* initializer_plugin_t::identity() const noexcept {
    return class_identity;
}

bool initializer_plugin_t::activate(actor_base_t *actor_) noexcept {
    actor = actor_;
    actor->install_plugin(*this, slot_t::INIT);
    actor->install_plugin(*this, slot_t::SUBSCRIPTION);
    actor->configure(*this);
    return plugin_t::activate(actor);
}

bool initializer_plugin_t::deactivate() noexcept {
    tracked.clear();
    return plugin_t::deactivate();
}


processing_result_t initializer_plugin_t::handle_subscription(message::subscription_t& message) noexcept {
    tracked.remove(message.payload.point);
    if (tracked.empty()) {
        actor->init_continue();
        return processing_result_t::FINISHED;
    }
    return processing_result_t::IGNORED;
}

bool initializer_plugin_t::handle_init(message::init_request_t*) noexcept {
    return tracked.empty();
}
