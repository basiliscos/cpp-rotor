#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/supervisor_config.h"
#include <ev.h>

namespace rotor {
namespace ev {

/** \struct supervisor_config_ev_t
 *  \brief libev supervisor config, which holds  a pointer to the ev
 * event loop and a loop ownership flag
 */
struct supervisor_config_ev_t : public supervisor_config_t {
    /** \brief a pointer to EV event loop */
    struct ev_loop *loop;

    /** \brief whether loop should be destroyed by supervisor */
    bool loop_ownership;

    using supervisor_config_t::supervisor_config_t;
};

template <typename Supervisor> struct supervisor_config_ev_builder_t : supervisor_config_builder_t<Supervisor> {
    using parent_t = supervisor_config_builder_t<Supervisor>;
    using parent_t::parent_t;

    constexpr static const std::uint32_t LOOP = 1 << 2;
    constexpr static const std::uint32_t LOOP_OWNERSHIP = 1 << 3;
    constexpr static const std::uint32_t requirements_mask = parent_t::requirements_mask | LOOP | LOOP_OWNERSHIP;

    supervisor_config_ev_builder_t &&loop(struct ev_loop *loop) && {
        parent_t::config.loop = loop;
        parent_t::mask = (parent_t::mask & ~LOOP);
        return std::move(*static_cast<typename parent_t::builder_t *>(this));
    }
    supervisor_config_ev_builder_t &&loop_ownership(bool value) && {
        parent_t::config.loop_ownership = value;
        parent_t::mask = (parent_t::mask & ~LOOP_OWNERSHIP);
        return std::move(*static_cast<typename parent_t::builder_t *>(this));
    }
};

} // namespace ev
} // namespace rotor
