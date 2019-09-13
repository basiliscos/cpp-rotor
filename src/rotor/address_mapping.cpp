//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/address_mapping.h"
#include "rotor/handler.hpp"
#include <cassert>

using namespace rotor;

void address_mapping_t::set(actor_base_t &actor, const void *message, const handler_ptr_t &handler,
                            const address_ptr_t &dest_addr) noexcept {
    auto &point_map = actor_map[static_cast<const void *>(&actor)];
    point_map.try_emplace(message, point_t{handler, dest_addr});
}

address_ptr_t address_mapping_t::get_addr(actor_base_t &actor, const void *message) noexcept {
    auto it_points = actor_map.find(static_cast<const void *>(&actor));
    if (it_points == actor_map.end()) {
        return address_ptr_t();
    }

    auto &point_map = it_points->second;
    auto it_subscription = point_map.find(message);
    if (it_subscription == point_map.end()) {
        return address_ptr_t();
    }

    return it_subscription->second.address;
}

address_mapping_t::points_t address_mapping_t::destructive_get(actor_base_t &actor) noexcept {
    auto it_points = actor_map.find(static_cast<const void *>(&actor));
    if (it_points == actor_map.end()) {
        return points_t{};
    }

    points_t points{};
    auto &point_map = it_points->second;
    for (auto it : point_map) {
        points.push_back(it.second);
    }
    actor_map.erase(it_points);
    return points;
}
