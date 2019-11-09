#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <boost/date_time/posix_time/posix_time.hpp>
#include "policy.h"

namespace rotor {

namespace pt = boost::posix_time;

/** \struct supervisor_config_t
 *  \brief base supervisor config, which holds shutdowm timeout value
 */
struct supervisor_config_t {
    /** \brief how much time is allowed to spend in shutdown for children actor */
    pt::time_duration shutdown_timeout;

    /** \brief how to behave if child-actor fails */
    supervisor_policy_t policy = supervisor_policy_t::shutdown_self;
};

} // namespace rotor
