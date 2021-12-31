//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/address_mapping.h"
#include "rotor/handler.h"
#include <cassert>

using namespace rotor;

void address_mapping_t::set(actor_base_t &actor, const subscription_info_ptr_t &info) noexcept {
    auto &point_map = actor_map[static_cast<const void *>(&actor)];
    auto message_type = info->handler->message_type;
    point_map.try_emplace(message_type, info);
}

address_ptr_t address_mapping_t::get_mapped_address(actor_base_t &actor, const void *message) noexcept {
    try {
        auto &point_map = actor_map.at(static_cast<const void *>(&actor));
        auto &info = point_map.at(message);
        return info->address;
    } catch (std::out_of_range &ex) {
        return {};
    }
}

void address_mapping_t::remove(const subscription_point_t &point) noexcept {
    auto &subs = actor_map.at(point.owner_ptr);
    for (auto it = subs.begin(); it != subs.end();) {
        auto &info = *it->second;
        if (info.handler.get() == point.handler.get() && info.address == point.address) {
            subs.erase(it);
            break;
        } else {
            ++it;
        }
    }
    if (subs.empty()) {
        actor_map.erase(point.owner_ptr);
    }
}

/*
void address_mapping_t::clear(supervisor_t&) noexcept {
    assert(actor_map.size() == 1);
    actor_map.clear();
}
*/

bool address_mapping_t::has_subscriptions(const actor_base_t &actor) const noexcept {
    try {
        actor_map.at(static_cast<const void *>(&actor));
        return true;
    } catch (std::out_of_range &ex) {
        return false;
    }
}
