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

/** \struct subscription_point_t
 *  \brief pair of {@link handler_base_t} linked to particular {@link address_t}
 */
struct subscription_point_t {
    /** \brief intrusive pointer to messages' handler */
    handler_ptr_t handler;
    /** \brief intrusive pointer to address */
    address_ptr_t address;

    inline bool operator==(const subscription_point_t& other) const noexcept {
        return address == other.address && handler == other.handler;
    }
};

/** \brief alias to the list of {@link subscription_point_t} */
using subscription_points_t = std::list<subscription_point_t>;


}
