//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/subscription_support.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::internal;

void subscription_support_plugin_t::activate(actor_base_t* actor_) noexcept {
    actor = actor_;
    auto& sup = static_cast<supervisor_t&>(*actor_);

    subscribe(&subscription_support_plugin_t::on_call);
    subscribe(&subscription_support_plugin_t::on_unsubscription);
    subscribe(&subscription_support_plugin_t::on_unsubscription_external);
    sup.subscription_support = this;
    plugin_t::activate(actor_);
}

void subscription_support_plugin_t::deactivate() noexcept {
    auto& sup = static_cast<supervisor_t&>(*actor);
    sup.subscription_support = nullptr;
    plugin_t::deactivate();
}

void subscription_support_plugin_t::on_call(message::handler_call_t &message) noexcept {
    auto &handler = message.payload.handler;
    auto &orig_message = message.payload.orig_message;
    handler->call(orig_message);
}

void subscription_support_plugin_t::on_unsubscription(message::commit_unsubscription_t &message) noexcept {
    auto& sup = static_cast<supervisor_t&>(*actor);
    auto& payload = message.payload;
    sup.commit_unsubscription(payload.target_address, payload.handler);
}

void subscription_support_plugin_t::on_unsubscription_external(message::external_subscription_t &message) noexcept {
    auto& sup = static_cast<supervisor_t&>(*actor);
    auto &handler = message.payload.handler;
    auto &addr = message.payload.target_address;
    assert(&addr->supervisor == &sup);
    sup.subscribe_actor(addr, handler);
}
