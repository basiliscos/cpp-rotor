#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <ev.h>

namespace rotor {
namespace ev {

struct supervisor_config_t {
    /** \brief type to represent time in seconds, used for shutdown timeout timer */
    using duration_t = ev_tstamp;

    /** \brief a pointer to EV event loop */
    struct ev_loop *loop;

    /** \brief whether loop should be destroyed */
    bool loop_ownership;

    /** \brief shutdown timeout value*/
    duration_t shutdown_timeout;
};

} // namespace ev
} // namespace rotor
