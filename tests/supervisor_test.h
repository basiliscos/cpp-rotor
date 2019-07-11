#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/supervisor.h"

namespace rotor {
namespace test {

struct supervisor_test_t : public supervisor_t {
    supervisor_test_t(supervisor_t *sup = nullptr);

    virtual void start_shutdown_timer() noexcept override;
    virtual void cancel_shutdown_timer() noexcept override;
    virtual void start() noexcept override;
    virtual void shutdown() noexcept override;
    virtual void enqueue(rotor::message_ptr_t message) noexcept override;

    state_t& get_state() noexcept { return  state; }
    queue_t& get_queue() noexcept { return outbound; }
    subscription_points_t& get_points() noexcept { return points; }
    subscription_map_t& get_subscription() noexcept { return subscription_map; }
};


} // namespace test
} // namespace rotor
