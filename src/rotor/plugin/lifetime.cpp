//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/lifetime.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::internal;

const void* lifetime_plugin_t::class_identity = static_cast<const void *>(typeid(lifetime_plugin_t).name());

const void* lifetime_plugin_t::identity() const noexcept {
    return class_identity;
}

void lifetime_plugin_t::activate(actor_base_t* actor_) noexcept {
    this->actor = actor_;

    actor->install_plugin(*this, slot_t::SHUTDOWN);
    actor->install_plugin(*this, slot_t::SUBSCRIPTION);
    actor->install_plugin(*this, slot_t::UNSUBSCRIPTION);

    actor->lifetime = this;
    // order is important
    subscribe(&lifetime_plugin_t::on_unsubscription);
    subscribe(&lifetime_plugin_t::on_unsubscription_external);
    subscribe(&lifetime_plugin_t::on_subscription);

    return plugin_t::activate(actor_);
}


void lifetime_plugin_t::deactivate() noexcept  {
    // NOOP, do not unsubscribe too early.
}

bool lifetime_plugin_t::handle_shutdown(message::shutdown_request_t*) noexcept {
    if (points.empty()) return true;
    unsubscribe();
    return false;
}

void lifetime_plugin_t::unsubscribe(const handler_ptr_t &h, const address_ptr_t &addr, const payload::callback_ptr_t &callback) noexcept {
    auto &dest = h->actor_ptr->address;
    auto point = subscription_point_t{h, addr};
    if (&addr->supervisor == actor) {
        actor->send<payload::unsubscription_confirmation_t>(dest, point, callback);
    } else {
        assert(!callback);
        actor->send<payload::external_unsubscription_t>(dest, point);
    }
}

void lifetime_plugin_t::unsubscribe() noexcept {
    auto& sup = actor->get_supervisor();
    auto rit = points.rbegin();
    while (rit != points.rend()) {
        auto& info = *rit;
        sup.unsubscribe_actor(info->address, info->handler);
        ++rit;
    }
}

void lifetime_plugin_t::on_subscription(message::subscription_t &msg) noexcept {
    actor->on_subscription(msg);
}

void lifetime_plugin_t::on_unsubscription(message::unsubscription_t &msg) noexcept {
    actor->on_unsubscription(msg);
}

void lifetime_plugin_t::on_unsubscription_external(message::unsubscription_external_t &msg) noexcept {
    actor->on_unsubscription_external(msg);
}

processing_result_t lifetime_plugin_t::remove_subscription(subscription_container_t::iterator it) noexcept {
    points.erase(it);
    if (points.empty()) {
        actor->shutdown_continue();
        actor->lifetime = nullptr;
        plugin_t::deactivate();
        return processing_result_t::FINISHED;
    }
    return processing_result_t::CONSUMED;
}

void lifetime_plugin_t::initate_subscription(const subscription_info_ptr_t& info) noexcept {
    points.emplace_back(info);
}

processing_result_t lifetime_plugin_t::handle_subscription(message::subscription_t& message) noexcept {
    auto& point = message.payload.point;
    // printf("subscribed %p to %s\n", point.address.get(), point.handler->message_type);
    auto predicate = [&point](auto& info) {
        return info->handler == point.handler && info->address == point.address;
    };
    auto rit = std::find_if(points.rbegin(), points.rend(), predicate);
    assert(rit != points.rend());
    auto it = --rit.base();
    auto& info = **it;
    assert(info.state == subscription_info_t::state_t::SUBSCRIBING || info.internal_address);
    if (!info.internal_address) info.state = subscription_info_t::state_t::SUBSCRIBED;
    return processing_result_t::CONSUMED;
}

processing_result_t lifetime_plugin_t::handle_unsubscription(message::unsubscription_t& message) noexcept {
    auto& point = message.payload.point;
    // printf("unsubscribed %p to %s\n", point.address.get(), point.handler->message_type);
    auto predicate = [&point](auto& info) { return info->handler == point.handler && info->address == point.address; };
    auto rit = std::find_if(points.rbegin(), points.rend(), predicate);
    assert(rit != points.rend());
    auto it = --rit.base();
    actor->get_supervisor().commit_unsubscription(*it);
    return remove_subscription(it);
}

processing_result_t lifetime_plugin_t::handle_unsubscription_external(message::unsubscription_external_t& message) noexcept {
    auto& point = message.payload.point;
    auto predicate = [&point](auto& info) { return info->handler == point.handler && info->address == point.address; };
    auto rit = std::find_if(points.rbegin(), points.rend(), predicate);
    assert(rit != points.rend());
    auto it = --rit.base();
    auto sup_addr = point.address->supervisor.get_address();
    actor->send<payload::commit_unsubscription_t>(sup_addr, point);
    return remove_subscription(it);
}

