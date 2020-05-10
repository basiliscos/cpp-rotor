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

bool initializer_plugin_t::is_complete_for(slot_t slot) noexcept {
    if (slot == slot_t::INIT) return tracked.empty();
    std::abort();
}

bool initializer_plugin_t::is_complete_for(slot_t slot, const subscription_point_t& point) noexcept {
    if (slot == slot_t::SUBSCRIPTION) {
        tracked.remove(point);
        return tracked.empty();
    }
    std::abort();
}
