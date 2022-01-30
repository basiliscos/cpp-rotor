//
// Copyright (c) 2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/detail/child_info.h"
#include "rotor/actor_base.h"

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

spawn_demand_t child_info_t::next_spawn(bool abnormal_shutdown) noexcept {
    if (timer_id) {
        return demand::no{};
    }

    auto check_failure_escalation = [&]() -> bool {
        if (escalate_failure && abnormal_shutdown) {
            if (policy == restart_policy_t::always && !active) {
                active = false;
                return true;
            }
            if (policy == restart_policy_t::normal_only) {
                active = false;
                return true;
            }
        }
        return false;
    };

    if (check_failure_escalation()) {
        return demand::escalate_failure{};
    }

    if (!active) {
        return demand::no{};
    }

    if (policy == restart_policy_t::fail_only && !abnormal_shutdown) {
        active = false;
        return demand::no{};
    }

    if (policy == restart_policy_t::normal_only && abnormal_shutdown) {
        active = false;
        return demand::no{};
    }

    if (policy == restart_policy_t::ask_actor) {
        auto need_restart = actor->should_restart();
        if (!need_restart) {
            active = false;
            return demand::no{};
        }
    }

    auto now = clock_t::local_time();
    auto after = now - last_instantiation;
    if (after >= restart_period) {
        return demand::now{};
    }
    return after;
}
