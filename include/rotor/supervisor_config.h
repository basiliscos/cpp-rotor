#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <boost/date_time/posix_time/posix_time.hpp>
#include "policy.h"
#include "actor_config.h"

namespace rotor {

namespace pt = boost::posix_time;

/** \struct supervisor_config_t
 *  \brief base supervisor config, which holds shutdowm timeout value
 */
struct supervisor_config_t : actor_config_t {

    /** \brief how to behave if child-actor fails */
    supervisor_policy_t policy = supervisor_policy_t::shutdown_self;

    supervisor_config_t(supervisor_t *parent, pt::time_duration init_timeout_, pt::time_duration shutdown_timeout_,
                        supervisor_policy_t policy_ = supervisor_policy_t::shutdown_self)
        :

          actor_config_t{parent, init_timeout_, shutdown_timeout_}, policy{policy_} {}
};

} // namespace rotor
