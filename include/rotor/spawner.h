#pragma once

//
// Copyright (c) 2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "forward.hpp"
#include "policy.h"

namespace rotor {

namespace details {
enum class request_state_t { NONE, SENT, CONFIRMED };
}

struct spawner_t {
    ~spawner_t();
    spawner_t&& restart_period(const pt::time_duration&) noexcept;
    spawner_t&& restart_policy(restart_policy_t) noexcept;
    spawner_t&& max_attempts(size_t) noexcept;
    void spawn() noexcept;

private:
    spawner_t(factory_t factory, supervisor_t& supervisor) noexcept;

    factory_t factory;
    supervisor_t& supervisor;
    pt::time_duration period = pt::seconds{15};
    restart_policy_t policy = restart_policy_t::normal_only;
    size_t attempts = 0;
    bool done = false;

    friend class supervisor_t;
};

}
