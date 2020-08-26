#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/forward.hpp"
#include "rotor/address.hpp"
#include <vector>
#include <list>

namespace rotor {

struct actor_base_t;

enum class owner_tag_t { PLUGIN, SUPERVISOR, FOREIGN, ANONYMOUS, NOT_AVAILABLE };

/** \struct subscription_point_t
 *  \brief pair of {@link handler_base_t} linked to particular {@link address_t}
 */
struct subscription_point_t {
    /** \brief intrusive pointer to messages' handler */
    handler_ptr_t handler;

    /** \brief intrusive pointer to address */
    address_ptr_t address;
    const actor_base_t *owner_ptr;

    owner_tag_t owner_tag;

    subscription_point_t(const handler_ptr_t &handler_, const address_ptr_t &address_) noexcept
        : handler{handler_}, address{address_}, owner_ptr{nullptr}, owner_tag{owner_tag_t::NOT_AVAILABLE} {}

    subscription_point_t(const handler_ptr_t &handler_, const address_ptr_t &address_, const actor_base_t *owner_ptr_,
                         owner_tag_t owner_tag_) noexcept
        : handler{handler_}, address{address_}, owner_ptr{owner_ptr_}, owner_tag{owner_tag_} {}

    subscription_point_t(const subscription_point_t &) = default;
    subscription_point_t(subscription_point_t &&) = default;

    bool operator==(const subscription_point_t &other) const noexcept;
};

/** \struct subscription_info_t
 *  \brief {@link subscription_point_t} with extended information (e.g. state)
 */
struct subscription_info_t : public arc_base_t<subscription_info_t>, subscription_point_t {
    enum state_t { SUBSCRIBING, SUBSCRIBED, UNSUBSCRIBING };

    subscription_info_t(const subscription_point_t &point, bool internal_address_, bool internal_handler_,
                        state_t state_) noexcept
        : subscription_point_t{point}, internal_address{internal_address_},
          internal_handler{internal_handler_}, state{state_} {}
    ~subscription_info_t();

    inline bool operator==(const subscription_point_t &point) const noexcept {
        return (subscription_point_t &)(*this) == point;
    }

    bool internal_address;
    bool internal_handler;
    state_t state;
};
using subscription_info_ptr_t = intrusive_ptr_t<subscription_info_t>;

/** \struct subscription_container_t
 *  \brief list of {@link subscription_info_ptr_t} with possibility to find via {@link subscription_point_t}
 */
struct subscription_container_t : public std::list<subscription_info_ptr_t> {
    iterator find(const subscription_point_t &point) noexcept;
};

} // namespace rotor
