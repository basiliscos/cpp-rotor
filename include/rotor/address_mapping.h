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

/** \struct address_mapping_t
 * \brief NAT mechanism for `rotor`
 *
 * It plays the similar role as https://en.wikipedia.org/wiki/Network_address_translation
 * in routers, i.e. the original message (request) is dispatched from different address
 * then source actor. It's needed for supervising it.
 *
 * The `address_mapping_t` does not hold the original reply address. That kind of
 * mapping is done on `per-request` basis. The `address_mapping_t` performs only
 * `per-message-type` mapping.
 *
 */
struct address_mapping_t {
    /** \brief alias for actor's subscription point */
    using point_t = typename actor_base_t::subscription_point_t;

    /** \brief alias for vector of subscription points */
    using points_t = std::vector<point_t>;

    /** \brief associates temporal destination point with actor's message type
     *
     * An actor is able to process message type indetified by `message`. So,
     * the temporal subscription point (hander and temporal address) will
     * be associated with the actor/message type pair.
     *
     * In the routing the temporal destination address is usually some
     * supervisor's address.
     *
     */
    void set(actor_base_t &actor, const void *message, const handler_ptr_t &handler,
             const address_ptr_t &dest_addr) noexcept;

    /** \brief returns temporal destination address for the actor/message type */
    address_ptr_t get_addr(actor_base_t &actor, const void *message) noexcept;

    /** \brief returns all subscription points for the actor
     *
     * All subscription points are removed. This needed for clean-up, i.e. once
     * an actor is removed, all related mappings for it should also be removed too.
     *
     */
    points_t destructive_get(actor_base_t &actor) noexcept;

  private:
    using point_map_t = std::unordered_map<const void *, point_t>;
    using actor_map_t = std::unordered_map<const void *, point_map_t>;
    actor_map_t actor_map;
};

} // namespace rotor
