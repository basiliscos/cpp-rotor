//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/init_shutdown.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::internal;

void init_shutdown_plugin_t::activate(actor_base_t* actor_) noexcept {
    actor = actor_;
    subscribe(&init_shutdown_plugin_t::on_shutdown);
    subscribe(&init_shutdown_plugin_t::on_init);

    actor->install_plugin(*this, slot_t::INIT);
    actor->install_plugin(*this, slot_t::SHUTDOWN);
    actor->init_shutdown_plugin = this;
    plugin_t::activate(actor);
}

bool init_shutdown_plugin_t::is_complete_for(slot_t slot) noexcept {
    if (slot == slot_t::INIT) return (bool)init_request;
    return true;
}

void init_shutdown_plugin_t::deactivate() noexcept {
    actor->init_shutdown_plugin = nullptr;
    plugin_t::deactivate();
}

void init_shutdown_plugin_t::confirm_init() noexcept {
    assert(init_request);
    actor->reply_to(*init_request);
    init_request.reset();
}

void init_shutdown_plugin_t::on_init(message::init_request_t& msg) noexcept {
    init_request.reset(&msg);
    actor->init_start();
}

void init_shutdown_plugin_t::on_shutdown(message::shutdown_request_t& msg) noexcept {
    assert(actor->state == state_t::OPERATIONAL);
    actor->state = state_t::SHUTTING_DOWN;
    actor->shutdown_request.reset(&msg);
    actor->shutdown_continue();
}

bool init_shutdown_plugin_t::handle_shutdown(message::shutdown_request_t&) noexcept {
    actor->deactivate_plugins();
    return true;
}
