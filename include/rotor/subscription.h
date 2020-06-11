#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//


#include "rotor/forward.hpp"
#include "rotor/address.hpp"
#include "rotor/subscription_point.h"
#include "rotor/message.h"
#include <map>
#include <vector>

namespace rotor {

struct subscription_info_t: public arc_base_t<subscription_info_t> {
    enum state_t { SUBSCRIBING, SUBSCRIBED, UNSUBSCRIBING };

    subscription_info_t(const handler_ptr_t& handler, const address_ptr_t& address,
                        bool internal_address, bool internal_handler, state_t state) noexcept;
    ~subscription_info_t();

    handler_ptr_t handler;
    address_ptr_t address;
    bool internal_address;
    bool internal_handler;
    state_t state;
};
using subscription_info_ptr_t = intrusive_ptr_t<subscription_info_t>;


/* \struct subscription_t
 *  \brief Holds and classifies message handlers on behalf of supervisor
 *
 * The handlers are classified by message type and by the source supervisor, i.e.
 * whether the hander's supervisor is external or not.
 *
 */
struct subscription_t {
    using message_type_t = const void *;
    using handlers_t = std::vector<handler_base_t*>;
    struct joint_handlers_t {
         handlers_t internal;
         handlers_t external;
    };

    struct subscrption_key_t {
        address_t* address;
        message_type_t message_type;
        inline bool operator==(const subscrption_key_t& other) const noexcept {
            return address == other.address && message_type == other.message_type;
        }
    };

    struct subscrption_key_hash_t {
        inline std::size_t operator()(const subscrption_key_t& k) const noexcept {
            return std::size_t(k.address) + 0x9e3779b9 + (size_t(k.message_type) << 6) + (size_t(k.message_type) >> 2);
        }
    };

    using addressed_handlers_t = std::unordered_map<subscrption_key_t, joint_handlers_t, subscrption_key_hash_t>;

    subscription_t(supervisor_t &supervisor) noexcept;

    subscription_info_ptr_t materialize(const handler_ptr_t& handler, const address_ptr_t& address) noexcept;

    void forget(const subscription_info_ptr_t& info) noexcept;

    const joint_handlers_t* get_recipients(const message_base_t& message) const noexcept;

    bool empty() const noexcept;

private:
    using info_container_t = std::unordered_map<address_ptr_t, std::vector<subscription_info_ptr_t>>;
    supervisor_t &supervisor;
    info_container_t internal_infos;
    info_container_t external_infos;
    addressed_handlers_t mine_handlers;
};

struct subscription_container_t: public std::list<subscription_info_ptr_t> {
      iterator find(const subscription_point_t& point) noexcept;
};

} // namespace rotor
