//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/address_mapping.h"
#include "rotor/handler.hpp"
#include <cassert>

using namespace rotor;

void address_mapping_t::set(actor_base_t &actor, const subscription_info_ptr_t &info) noexcept {
    auto &point_map = actor_map[static_cast<const void *>(&actor)];
    auto message_type = info->handler->message_type;
    point_map.try_emplace(message_type, info);
}

address_ptr_t address_mapping_t::get_mapped_address(actor_base_t &actor, const void *message) noexcept {
    auto it_points = actor_map.find(static_cast<const void *>(&actor));
    if (it_points == actor_map.end()) {
        return address_ptr_t();
    }

    auto &point_map = it_points->second;
    auto it_subscription = point_map.find(message);
    if (it_subscription == point_map.end()) {
        return address_ptr_t();
    }

    return it_subscription->second->address;
}

void address_mapping_t::remove(const subscription_point_t& point) noexcept {
    auto it_points = actor_map.find(point.owner_ptr);
    if (it_points == actor_map.end()) return;

    auto& subs = it_points->second;
    for(auto it = subs.begin(); it != subs.end(); ) {
        auto& info = *it->second;
        if (info.handler.get() == point.handler.get() && info.address == point.address) {
            subs.erase(it);
            break;
        } else {
            ++it;
        }
    }
    if (subs.empty()) { actor_map.erase(it_points); }
}

/*
void address_mapping_t::clear(supervisor_t&) noexcept {
    assert(actor_map.size() == 1);
    actor_map.clear();
}
*/

bool address_mapping_t::has_subscriptions(const actor_base_t &actor) const noexcept {
    auto it = actor_map.find(static_cast<const void *>(&actor));
    return it != actor_map.end();
}
