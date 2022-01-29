//
// Copyright (c) 2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/detail/child_info.h"

using namespace rotor::detail;

void child_info_t::spawn_attempt() noexcept {
    initialized = started = false;
    ++attempts;
    timer_id = 0;
    shutdown = shutdown_state_t::none;
    last_instantiation = clock_t::local_time();
    if (max_attempts && attempts >= max_attempts) {
        active = false;
    }
    if (policy == restart_policy_t::never) {
        active = false;
    }
}

spawn_demand_t child_info_t::can_spawn() noexcept {
    if (actor || !active || timer_id) {
        return demand::no{};
    }
    auto now = clock_t::local_time();
    auto after = now - last_instantiation;
    if (after >= restart_period) {
        return demand::now{};
    }
    return after;
}
