#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/asio/supervisor_asio.h"

namespace rotor {
namespace test {

struct supervisor_asio_test_t : public rotor::asio::supervisor_asio_t {
    using rotor::asio::supervisor_asio_t::supervisor_asio_t;

    timers_map_t& get_timers_map() noexcept { return timers_map; }
    state_t &get_state() noexcept { return state; }
    queue_t& get_leader_queue() { return get_leader().queue; }
    supervisor_asio_test_t& get_leader() { return *static_cast<supervisor_asio_test_t*>(locality_leader); }
    subscription_points_t &get_points() noexcept { return points; }
    subscription_map_t &get_subscription() noexcept { return subscription_map; }
};

} // namespace test
} // namespace rotor
