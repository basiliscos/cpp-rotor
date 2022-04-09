#pragma once

//
// Copyright (c) 2019-2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/forward.hpp"
#include "rotor/address.hpp"
#include "rotor/subscription_point.h"
#include "rotor/message.h"
#include <unordered_map>
#include <vector>

namespace rotor {

/** \struct subscription_t
 *  \brief Holds and classifies message handlers on behalf of supervisor
 *
 * The handlers are classified by message type and by the source supervisor, i.e.
 * whether the hander's supervisor is external or not.
 *
 */
struct ROTOR_API subscription_t {
    /** \brief alias for message type (i.e. stringized typeid) */
    using message_type_t = const void *;

    /** \brief vector of handler pointers */
    using handlers_t = std::vector<handler_base_t *>;

    /** \struct joint_handlers_t
     *  \brief pair internal and external {@link handler_t}
     */
    struct joint_handlers_t {
        /** \brief internal handlers, i.e. those which belong to actors of the supervisor */
        handlers_t internal;
        /** \brief external handlers, i.e. those which belong to actors of other supervisor */
        handlers_t external;
    };

    subscription_t() noexcept;

    /** \brief upgrades subscription_point_t into subscription_info smart pointer
     *
     * Performs the classification of the point, i.e. whether handler and address
     * are internal or external, records the state subcription state and records
     * the handler among address handlers.
     *
     */
    subscription_info_ptr_t materialize(const subscription_point_t &point) noexcept;

    /** \brief sets new handler for the subscription point */
    void update(subscription_point_t &point, handler_ptr_t &new_handler) noexcept;

    /** \brief remove subscription_info from `internal_infos` and `mine_handlers` */
    void forget(const subscription_info_ptr_t &info) noexcept;

    /** \brief returns list of all handlers for the message (internal and external) */
    const joint_handlers_t *get_recipients(const message_base_t &message) const noexcept;

    /** \brief generic non-public fields accessor */
    template <typename T> auto &access() noexcept;

  private:
    struct subscrption_key_t {
        address_t *address;
        message_type_t message_type;
        inline bool operator==(const subscrption_key_t &other) const noexcept {
            return address == other.address && message_type == other.message_type;
        }
    };

    struct subscrption_key_hash_t {
        inline std::size_t operator()(const subscrption_key_t &k) const noexcept {
            return std::size_t(k.address) + 0x9e3779b9 + (size_t(k.message_type) << 6) + (size_t(k.message_type) >> 2);
        }
    };

    using addressed_handlers_t = std::unordered_map<subscrption_key_t, joint_handlers_t, subscrption_key_hash_t>;

    using info_container_t = std::unordered_map<address_ptr_t, std::vector<subscription_info_ptr_t>>;
    address_t *main_address;
    info_container_t internal_infos;
    addressed_handlers_t mine_handlers;
};

} // namespace rotor
