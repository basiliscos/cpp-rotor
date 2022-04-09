#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "arc.hpp"
#include "subscription.h"
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
struct ROTOR_API address_mapping_t {
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
    void set(actor_base_t &actor, const subscription_info_ptr_t &info) noexcept;

    /** \brief returns temporal destination address for the actor/message type */
    address_ptr_t get_mapped_address(actor_base_t &actor, const void *) noexcept;

    /** \brief iterates on all subscriptions for an actor */
    template <typename Fn> void each_subscription(const actor_base_t &actor, Fn &&fn) const noexcept {
        auto it_mappings = actor_map.find(static_cast<const void *>(&actor));
        for (auto it : it_mappings->second) {
            fn(it.second);
        }
    }

    /** \brief checks whether an actor has any subscriptions */
    bool has_subscriptions(const actor_base_t &actor) const noexcept;

    /** \brief returns true if there is no any subscription for any actor */
    bool empty() const noexcept { return actor_map.empty(); }

    /** \brief forgets subscription point */
    void remove(const subscription_point_t &point) noexcept;

  private:
    using point_map_t = std::unordered_map<const void *, subscription_info_ptr_t>;
    using actor_map_t = std::unordered_map<const void *, point_map_t>;
    actor_map_t actor_map;
};

} // namespace rotor
