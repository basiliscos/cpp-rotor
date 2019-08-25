#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <ev.h>

namespace rotor {
namespace ev {

/** \struct supervisor_config_t
 *  \brief libev supervisor config, which holds shutdowm timeout value,
 * a pointer to the ev event loop and a loop ownership flag
 */
struct supervisor_config_t {
    /** \brief a pointer to EV event loop */
    struct ev_loop *loop;

    /** \brief whether loop should be destroyed by supervisor */
    bool loop_ownership;
};

} // namespace ev
} // namespace rotor
