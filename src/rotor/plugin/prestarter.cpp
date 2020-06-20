//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/starter.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::internal;

const void* prestarter_plugin_t::class_identity = static_cast<const void *>(typeid(prestarter_plugin_t).name());

const void* prestarter_plugin_t::identity() const noexcept {
    return class_identity;
}

void prestarter_plugin_t::activate(actor_base_t *actor_) noexcept {
    plugin_t::activate(actor_);
    actor->install_plugin(*this, slot_t::INIT);
    actor->install_plugin(*this, slot_t::SUBSCRIPTION);
    actor->configure(*this);
    if (tracked.empty()) {
        actor->uninstall_plugin(*this, slot_t::INIT);
        actor->uninstall_plugin(*this, slot_t::SUBSCRIPTION);
    }
}

processing_result_t prestarter_plugin_t::handle_subscription(message::subscription_t& message) noexcept {
    auto& point = message.payload.point;
    auto it = std::find_if(tracked.begin(), tracked.end(), [&](auto info){ return *info == point; } );
    if (it != tracked.end()) {
        tracked.erase(it);
    }
    if (tracked.empty()) {
        actor->init_continue();
        return processing_result_t::FINISHED;
    }
    return processing_result_t::IGNORED;
}

bool prestarter_plugin_t::handle_init(message::init_request_t* request) noexcept {
    return tracked.empty() && request;
}
