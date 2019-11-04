#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/supervisor.h"
#include <list>

namespace rotor {
namespace test {

struct supervisor_config_test_t: public supervisor_config_t  {
    const void *locality;

    supervisor_config_test_t(const pt::time_duration& shutdown_timeout_, const void *locality_,
                             supervisor_policy_t policy_ = supervisor_policy_t::shutdown_self):
        supervisor_config_t {shutdown_timeout_, policy_}, locality{locality_} {

    }
};

struct supervisor_test_t : public supervisor_t {
    using timers_t = std::list<timer_id_t>;
    supervisor_test_t(supervisor_t *sup, const supervisor_config_test_t& config_);

    virtual void start_timer(const pt::time_duration &send, timer_id_t timer_id) noexcept override;
    virtual void cancel_timer(timer_id_t timer_id) noexcept override;
    timer_id_t get_timer(std::size_t index) noexcept;
    virtual void start() noexcept override;
    virtual void shutdown() noexcept override;
    virtual void enqueue(rotor::message_ptr_t message) noexcept override;
    virtual address_ptr_t make_address() noexcept override;

    state_t &get_state() noexcept { return state; }
    queue_t& get_leader_queue() { return get_leader().queue; }
    supervisor_test_t& get_leader() { return *static_cast<supervisor_test_t*>(locality_leader); }
    subscription_points_t &get_points() noexcept { return points; }
    subscription_map_t &get_subscription() noexcept { return subscription_map; }
    actors_map_t &get_children() noexcept { return actors_map; }
    request_map_t &get_requests() noexcept { return request_map; }

    const void *locality;
    timers_t active_timers;
};

} // namespace test
} // namespace rotor
