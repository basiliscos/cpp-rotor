//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/init_shutdown.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::internal;

const void* init_shutdown_plugin_t::class_identity = static_cast<const void *>(typeid(init_shutdown_plugin_t).name());

const void* init_shutdown_plugin_t::identity() const noexcept {
    return class_identity;
}

void init_shutdown_plugin_t::activate(actor_base_t* actor_) noexcept {
    actor = actor_;
    subscribe(&init_shutdown_plugin_t::on_init);
    subscribe(&init_shutdown_plugin_t::on_shutdown);

    actor->install_plugin(*this, slot_t::INIT);
    actor->install_plugin(*this, slot_t::SHUTDOWN);
}

void init_shutdown_plugin_t::on_init(message::init_request_t& msg) noexcept {
    /*
    assert(actor->state == state_t::NEW);
    actor->state = state_t::INITIALIZING;
    */
    actor->init_request.reset(&msg);
    plugin_t::activate(actor);
    actor->init_continue();
}

void init_shutdown_plugin_t::on_shutdown(message::shutdown_request_t& msg) noexcept {
    assert((actor->state != state_t::SHUTTING_DOWN) || (actor->state != state_t::SHUTTED_DOWN));
    actor->shutdown_request.reset(&msg);
    actor->shutdown_start();
    actor->shutdown_continue();
}

bool init_shutdown_plugin_t::handle_init(message::init_request_t*) noexcept {
    return bool(actor->init_request);
}


bool init_shutdown_plugin_t::handle_shutdown(message::shutdown_request_t*) noexcept {
    actor->deactivate_plugins();
    return true;
}

