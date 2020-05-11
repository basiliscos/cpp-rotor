//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/subscription.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::internal;

bool subscription_plugin_t::activate(actor_base_t* actor_) noexcept {
    this->actor = actor_;

    actor->install_plugin(*this, slot_t::SUBSCRIPTION);
    actor->install_plugin(*this, slot_t::UNSUBSCRIPTION);

    // order is important
    subscribe(&subscription_plugin_t::on_unsubscription);
    subscribe(&subscription_plugin_t::on_unsubscription_external);
    subscribe(&subscription_plugin_t::on_subscription);

    return plugin_t::activate(actor_);
}


bool subscription_plugin_t::deactivate() noexcept  {
    unsubscribe();
    return false;
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
    actor->on_subscription(msg);
}

void subscription_plugin_t::on_unsubscription(message::unsubscription_t &msg) noexcept {
    actor->on_unsubscription(msg);
}

void subscription_plugin_t::on_unsubscription_external(message::unsubscription_external_t &msg) noexcept {
    actor->on_unsubscription_external(msg);
}

processing_result_t subscription_plugin_t::remove_subscription(const subscription_point_t& point) noexcept {
    auto rit = std::find(points.rbegin(), points.rend(), point);
    assert(rit != points.rend());
    auto it = --rit.base();
    points.erase(it);
    if (points.empty()) {
        plugin_t::deactivate();
        return processing_result_t::FINISHED;
    }
    return processing_result_t::CONSUMED;
}


processing_result_t subscription_plugin_t::handle_subscription(message::subscription_t& message) noexcept {
    points.push_back(message.payload.point);
    return processing_result_t::CONSUMED;
}

processing_result_t subscription_plugin_t::handle_unsubscription(message::unsubscription_t& message) noexcept {
    auto& point = message.payload.point;
    actor->get_supervisor().commit_unsubscription(point.address, point.handler);
    return remove_subscription(point);
}

processing_result_t subscription_plugin_t::handle_unsubscription_external(message::unsubscription_external_t& message) noexcept {
    auto& point = message.payload.point;
    auto sup_addr = point.address->supervisor.get_address();
    actor->send<payload::commit_unsubscription_t>(sup_addr, point);
    return remove_subscription(point);
}

