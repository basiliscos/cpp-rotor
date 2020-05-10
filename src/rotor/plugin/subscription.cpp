//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/subscription.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::internal;

void subscription_plugin_t::activate(actor_base_t* actor_) noexcept {
    actor_->subscription_plugin = this;
    this->actor = actor_;

    actor->install_plugin(*this, slot_t::UNSUBSCRIPTION);
    // order is important
    subscribe(&subscription_plugin_t::on_unsubscription);
    subscribe(&subscription_plugin_t::on_unsubscription_external);
    subscribe(&subscription_plugin_t::on_subscription);

    plugin_t::activate(actor_);
}


subscription_plugin_t::iterator_t subscription_plugin_t::find_subscription(const address_ptr_t &addr, const handler_ptr_t &handler) noexcept {
    auto it = points.rbegin();
    while (it != points.rend()) {
        if (it->address == addr && *it->handler == *handler) {
            return --it.base();
        } else {
            ++it;
        }
    }
    assert(0 && "no subscription found");
}

bool subscription_plugin_t::is_complete_for(slot_t slot, const subscription_point_t&) noexcept {
    if (slot == slot_t::UNSUBSCRIPTION && points.empty() && actor->get_state() == state_t::SHUTTING_DOWN) {
        actor->subscription_plugin = nullptr;
        plugin_t::deactivate();
        return true;
    }
    return false;
}

void subscription_plugin_t::deactivate() noexcept  {
    unsubscribe();
}

void subscription_plugin_t::unsubscribe() noexcept {
    auto it = points.rbegin();
    auto& sup = actor->get_supervisor();
    while (it != points.rend()) {
        auto &addr = it->address;
        auto &handler = it->handler;
        sup.unsubscribe_actor(addr, handler);
        ++it;
    }
}

void subscription_plugin_t::on_subscription(message::subscription_t &msg) noexcept {
    auto point = subscription_point_t{msg.payload.handler, msg.payload.target_address};
    points.push_back(point);
    actor->on_subscription(point);
}

void subscription_plugin_t::on_unsubscription(message::unsubscription_t &msg) noexcept {
    auto &addr = msg.payload.target_address;
    auto &handler = msg.payload.handler;
    auto it = find_subscription(addr, handler);
    auto point = *it; /* copy */
    points.erase(it);
    actor->get_supervisor().commit_unsubscription(addr, handler);
    actor->on_unsubscription(point);
}

void subscription_plugin_t::on_unsubscription_external(message::unsubscription_external_t &msg) noexcept {
    auto &addr = msg.payload.target_address;
    auto &handler = msg.payload.handler;
    auto it = find_subscription(addr, handler);
    auto point = *it; /* copy */
    points.erase(it);
    auto sup_addr = addr->supervisor.get_address();
    actor->send<payload::commit_unsubscription_t>(sup_addr, addr, handler);
    actor->on_unsubscription(point);
}

