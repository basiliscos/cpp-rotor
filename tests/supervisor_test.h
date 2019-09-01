#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/supervisor.h"
#include <unordered_set>

namespace rotor {
namespace test {

struct supervisor_config_test_t: public supervisor_config_t  {
    const void *locality;

    supervisor_config_test_t(const pt::time_duration& shutdown_timeout_, const void *locality_): supervisor_config_t {shutdown_timeout_}, locality{locality_} {

    }
};

struct supervisor_test_t : public supervisor_t {
    using set_t = std::unordered_set<timer_id_t>;
    supervisor_test_t(supervisor_t *sup, const supervisor_config_test_t& config_);

    virtual void start_timer(const pt::time_duration &timeout, timer_id_t timer_id) noexcept override;
    virtual void cancel_timer(timer_id_t timer_id) noexcept override;
    virtual void start() noexcept override;
    virtual void shutdown() noexcept override;
    virtual void enqueue(rotor::message_ptr_t message) noexcept override;
    virtual address_ptr_t make_address() noexcept override;

    state_t &get_state() noexcept { return state; }
    queue_t &get_queue() noexcept { return *effective_queue; }
    subscription_points_t &get_points() noexcept { return points; }
    subscription_map_t &get_subscription() noexcept { return subscription_map; }
    actors_map_t &get_children() noexcept { return actors_map; }
    request_map_t &get_requests() noexcept { return request_map; }

    const void *locality;
    set_t active_timers;
};

} // namespace test
} // namespace rotor
