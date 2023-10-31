#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//
#include <catch2/catch_test_macros.hpp>
#include "rotor/asio/supervisor_asio.h"
#include "access.h"

namespace rotor {
namespace test {

struct supervisor_asio_test_t : public rotor::asio::supervisor_asio_t {
    using rotor::asio::supervisor_asio_t::supervisor_asio_t;

    timers_map_t &get_timers_map() noexcept { return timers_map; }
    state_t &get_state() noexcept { return state; }
    auto &get_leader_queue() { return access<to::locality_leader>()->access<to::queue>(); }

    subscription_t &get_subscription() noexcept { return subscription_map; }
};

} // namespace test
} // namespace rotor
