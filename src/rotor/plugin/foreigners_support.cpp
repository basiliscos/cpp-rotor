//
// Copyright (c) 2019-2023 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/foreigners_support.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::plugin;

namespace {
namespace to {
struct lifetime {};
struct points {};
struct state {};
struct alive_actors {};
} // namespace to
} // namespace

template <> auto &supervisor_t::access<to::alive_actors>() noexcept { return alive_actors; }
template <> auto &actor_base_t::access<to::lifetime>() noexcept { return lifetime; }
template <> auto &lifetime_plugin_t::access<to::points>() noexcept { return points; }
template <> auto &actor_base_t::access<to::state>() noexcept { return state; }

const std::type_index foreigners_support_plugin_t::class_identity = typeid(foreigners_support_plugin_t);

const std::type_index &foreigners_support_plugin_t::identity() const noexcept { return class_identity; }

void foreigners_support_plugin_t::activate(actor_base_t *actor_) noexcept {
    actor = actor_;

    subscribe(&foreigners_support_plugin_t::on_call);
    subscribe(&foreigners_support_plugin_t::on_unsubscription);
    subscribe(&foreigners_support_plugin_t::on_subscription_external);

    return plugin_base_t::activate(actor_);
}

void foreigners_support_plugin_t::deactivate() noexcept {
    auto lifetime = actor->access<to::lifetime>();
    for (auto &info : foreign_points) {
        lifetime->unsubscribe(info);
    }
    return plugin_base_t::deactivate();
}

void foreigners_support_plugin_t::on_call(message::handler_call_t &message) noexcept {
    auto &handler = message.payload.handler;
    auto child_actor = handler->actor_ptr;
    // need to check, that
    // 1. children exists
    // 2. check it's state
    // 2. it is still subscribed to the message

    auto &sup = static_cast<supervisor_t &>(*actor);
    if (sup.access<to::alive_actors>().count(child_actor)) {
        if (child_actor->access<to::state>() < state_t::SHUT_DOWN) {
            auto &orig_message = message.payload.orig_message;
            auto point = subscription_point_t(handler, orig_message->address);
            auto lifetime = child_actor->access<to::lifetime>();
            if (lifetime) {
                auto &points = lifetime->access<to::points>();
                if (points.find(point) != points.end()) {
                    handler->call(orig_message);
                }
            }
        }
    }
}

void foreigners_support_plugin_t::on_subscription_external(message::external_subscription_t &message) noexcept {
    auto &sup = static_cast<supervisor_t &>(*actor);
    auto &point = message.payload.point;
    assert(&point.address->supervisor == &sup);
    auto info = sup.subscribe(point.handler, point.address, point.owner_ptr, owner_tag_t::FOREIGN);
    foreign_points.emplace_back(info);
}

void foreigners_support_plugin_t::on_unsubscription(message::commit_unsubscription_t &message) noexcept {
    auto &sup = static_cast<supervisor_t &>(*actor);
    auto &point = message.payload.point;

    auto it = foreign_points.find(point);
    auto &info = *it;

    sup.commit_unsubscription(info);
    foreign_points.erase(it);

    if (foreign_points.empty() && actor->access<to::state>() == state_t::SHUTTING_DOWN) {
        plugin_base_t::deactivate();
    }
}
