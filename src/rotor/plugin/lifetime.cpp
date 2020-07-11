//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/lifetime.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::internal;

const void *lifetime_plugin_t::class_identity = static_cast<const void *>(typeid(lifetime_plugin_t).name());

const void *lifetime_plugin_t::identity() const noexcept { return class_identity; }

void lifetime_plugin_t::activate(actor_base_t *actor_) noexcept {
    this->actor = actor_;

    reaction_on(reaction_t::INIT);
    reaction_on(reaction_t::SHUTDOWN);

    actor->lifetime = this;
    // order is important
    subscribe(&lifetime_plugin_t::on_unsubscription);
    subscribe(&lifetime_plugin_t::on_unsubscription_external);
    subscribe(&lifetime_plugin_t::on_subscription);

    return plugin_t::activate(actor_);
}

void lifetime_plugin_t::deactivate() noexcept {
    // NOOP, do not unsubscribe too early.
}

bool lifetime_plugin_t::handle_shutdown(message::shutdown_request_t *) noexcept {
    if (points.empty() && get_subscriptions().empty())
        return true;
    unsubscribe();
    return false;
}

void lifetime_plugin_t::unsubscribe(const subscription_info_ptr_t &info_ptr) noexcept {
    using state_t = subscription_info_t::state_t;
    auto &info = *info_ptr;
    auto &handler = info.handler;
    auto &dest = handler->actor_ptr->address;
    // auto point = subscription_point_t{handler, info.address};
    if (info.state != state_t::UNSUBSCRIBING) {
        info.state = state_t::UNSUBSCRIBING;
        if (info.internal_address) {
            actor->send<payload::unsubscription_confirmation_t>(dest, info);
        } else {
            actor->send<payload::external_unsubscription_t>(dest, info);
        }
    }
}

void lifetime_plugin_t::unsubscribe() noexcept {
    auto rit = points.rbegin();
    while (rit != points.rend()) {
        auto &info = *rit;
        if (info->owner_tag != owner_tag_t::PLUGIN) {
            unsubscribe(info);
            ++rit;
        } else {
            auto it = points.erase(--rit.base());
            rit = std::reverse_iterator(it);
        }
    }
    /* wait only self to be deactivated */
    if (points.empty() && actor->ready_to_shutdown()) {
        plugin_t::deactivate();
        actor->lifetime = nullptr;
    }
}

void lifetime_plugin_t::on_subscription(message::subscription_t &msg) noexcept { actor->on_subscription(msg); }

void lifetime_plugin_t::on_unsubscription(message::unsubscription_t &msg) noexcept { actor->on_unsubscription(msg); }

void lifetime_plugin_t::on_unsubscription_external(message::unsubscription_external_t &msg) noexcept {
    actor->on_unsubscription_external(msg);
}

void lifetime_plugin_t::initate_subscription(const subscription_info_ptr_t &info) noexcept {
    points.emplace_back(info);
}

plugin_t::processing_result_t lifetime_plugin_t::handle_subscription(message::subscription_t &message) noexcept {
    auto &point = message.payload.point;
    // printf("[+]subscribed %p to %s\n", point.address.get(), point.handler->message_type);
    auto it = points.find(point);
    auto &info = **it;
    assert(info.state == subscription_info_t::state_t::SUBSCRIBING || info.internal_address);
    if (!info.internal_address)
        info.state = subscription_info_t::state_t::SUBSCRIBED;
    return processing_result_t::CONSUMED;
}

bool lifetime_plugin_t::handle_unsubscription(const subscription_point_t &point, bool external) noexcept {
    // printf("[-]unsubscribed %p to %s\n", point.address.get(), point.handler->message_type);
    subscription_info_ptr_t info;
    bool result = false;
    if (point.owner_tag != owner_tag_t::PLUGIN) {
        auto it = points.find(point);
        plugin_t::forget_subscription(*it);
        points.erase(it);
        result = true;
        if (points.empty()) {
            plugin_t::deactivate();
        }
    } else {
        result = plugin_t::handle_unsubscription(point, external);
    }
    return result;
}
