#pragma once

//
// Copyright (c) 2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/forward.hpp"
#include "rotor/policy.h"
#include <variant>

namespace rotor::detail {

enum class shutdown_state_t {none, sent, confirmed};

namespace demand {
    struct no{};
    struct now{};
}

using spawn_demand_t = std::variant<demand::no, demand::now, pt::time_duration>;

struct child_info_t: boost::intrusive_ref_counter<child_info_t, boost::thread_unsafe_counter> {
    using clock_t = pt::microsec_clock;

    template<typename Factory>
    child_info_t(address_ptr_t address_, Factory&& factory_, restart_policy_t policy_ = restart_policy_t::normal_only,
                 const pt::time_duration& restart_period_ =  pt::seconds{15}, size_t max_attempts_ = 0) noexcept :
        address{std::move(address_)},
        factory{std::forward<Factory>(factory_)},
        policy{policy_},
        restart_period{restart_period_},
        max_attempts{max_attempts_}
    {
    }

    template<typename Factory, typename Actor>
    child_info_t(address_ptr_t address_, Factory&& factory_, Actor&& actor_) noexcept :
        child_info_t(std::move(address_), std::forward<Factory>(factory_))
    {
        actor = std::forward<Actor>(actor_);
        if (actor) {
            spawn_attempt();
        }
    }

    void spawn_attempt() noexcept;

    spawn_demand_t next_spawn(bool abnormal_shutdown) noexcept;

    address_ptr_t address;
    factory_t factory;
    actor_ptr_t actor;
    restart_policy_t policy;
    pt::time_duration restart_period;
    shutdown_state_t shutdown = shutdown_state_t::none;
    size_t max_attempts;
    size_t attempts = 0;
    bool active = true;
    request_id_t timer_id{0};

    pt::ptime last_instantiation;
    bool initialized = false;
    bool started = false;
};

using child_info_ptr_t = boost::intrusive_ptr<child_info_t>;


}
