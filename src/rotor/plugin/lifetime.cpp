//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/lifetime.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::plugin;

namespace to {
struct state {};
struct internal_address {};
struct owner_tag {};
struct own_subscriptions {};
struct deactivating_plugins {};
} // namespace to

template <> auto &plugin_base_t::access<to::own_subscriptions>() noexcept { return own_subscriptions; }
template <> auto &actor_base_t::access<to::deactivating_plugins>() noexcept { return deactivating_plugins; }
template <> auto &subscription_info_t::access<to::state>() noexcept { return state; }
template <> auto &subscription_info_t::access<to::internal_address>() noexcept { return internal_address; }
template <> auto &subscription_info_t::access<to::owner_tag>() noexcept { return owner_tag; }

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

    return plugin_base_t::activate(actor_);
}

void lifetime_plugin_t::deactivate() noexcept {
    // NOOP, do not unsubscribe too early.
}

bool lifetime_plugin_t::handle_shutdown(message::shutdown_request_t *req) noexcept {
    if (points.empty() && plugin_base_t::access<to::own_subscriptions>().empty())
        return plugin_base_t::handle_shutdown(req);
    unsubscribe();
    return false;
}

void lifetime_plugin_t::unsubscribe(const subscription_info_ptr_t &info_ptr) noexcept {
    using state_t = subscription_info_t::state_t;
    auto &info = *info_ptr;
    auto &handler = info.handler;
    auto &dest = handler->actor_ptr->address;
    if (info.access<to::state>() != state_t::UNSUBSCRIBING) {
        info.access<to::state>() = state_t::UNSUBSCRIBING;

        if (info.access<to::owner_tag>() != owner_tag_t::FOREIGN) {
            if (info.access<to::internal_address>()) {
                actor->send<payload::unsubscription_confirmation_t>(dest, info);
            } else {
                actor->send<payload::external_unsubscription_t>(dest, info);
            }
        }
    }
}

void lifetime_plugin_t::unsubscribe(const handler_ptr_t &h, const address_ptr_t &addr) noexcept {
    auto it = points.find(subscription_point_t{h, addr});
    assert(it != points.end());
    unsubscribe(*it);
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
    if (points.empty() && ready_to_shutdown()) {
        plugin_base_t::deactivate();
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

bool lifetime_plugin_t::handle_unsubscription(const subscription_point_t &point, bool external) noexcept {
    // printf("[-]unsubscribed %p to %s\n", point.address.get(), point.handler->message_type);
    subscription_info_ptr_t info;
    bool result = false;
    if (point.owner_tag != owner_tag_t::PLUGIN) {
        auto it = points.find(point);
        plugin_base_t::forget_subscription(*it);
        points.erase(it);
        result = true;
        if (points.empty()) {
            plugin_base_t::deactivate();
        }
    } else {
        result = plugin_base_t::handle_unsubscription(point, external);
    }
    return result;
}

bool lifetime_plugin_t::ready_to_shutdown() noexcept {
    /* just me */
    return actor->access<to::deactivating_plugins>().size() == 1;
}
