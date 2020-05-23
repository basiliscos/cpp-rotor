//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/starter.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::internal;

const void* starter_plugin_t::class_identity = static_cast<const void *>(typeid(starter_plugin_t).name());

const void* starter_plugin_t::identity() const noexcept {
    return class_identity;
}

void starter_plugin_t::activate(actor_base_t *actor_) noexcept {
    plugin_t::activate(actor_);
    subscribe(&starter_plugin_t::on_start);
    actor->install_plugin(*this, slot_t::INIT);
}

bool starter_plugin_t::handle_init(message::init_request_t* req) noexcept {
    return (bool)req;
}


void starter_plugin_t::on_start(message::start_trigger_t&) noexcept {
    actor->on_start();
}
