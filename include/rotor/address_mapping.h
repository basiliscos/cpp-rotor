#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "arc.hpp"
#include "actor_base.h"
#include <unordered_map>
#include <vector>

namespace rotor {

struct address_mapping_t {
    using point_t = typename actor_base_t::subscription_point_t;
    using points_t = std::vector<point_t>;
    using point_map_t = std::unordered_map<const void *, point_t>;

    void set(actor_base_t &actor, const void *message, const handler_ptr_t &handler,
             const address_ptr_t &dest_addr) noexcept;

    address_ptr_t get_addr(actor_base_t &actor, const void *message) noexcept;
    points_t destructive_get(actor_base_t &actor) noexcept;

  private:
    using actor_map_t = std::unordered_map<const void *, point_map_t>;
    actor_map_t actor_map;
};

} // namespace rotor
