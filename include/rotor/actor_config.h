#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <boost/date_time/posix_time/posix_time.hpp>

namespace rotor {

struct supervisor_t;

namespace pt = boost::posix_time;

struct actor_config_t {
    supervisor_t *supervisor;

    pt::time_duration init_timeout;

    /** \brief how much time is allowed to spend in shutdown for children actor */
    pt::time_duration shutdown_timeout;

    actor_config_t(const pt::time_duration &init_timeout_, const pt::time_duration &shutdown_timeout_)
        : supervisor{nullptr}, init_timeout{init_timeout_}, shutdown_timeout{shutdown_timeout_} {}

    actor_config_t(supervisor_t *supervisor_, const pt::time_duration &init_timeout_,
                   const pt::time_duration &shutdown_timeout_)
        : supervisor{supervisor_}, init_timeout{init_timeout_}, shutdown_timeout{shutdown_timeout_} {}
};

} // namespace rotor
