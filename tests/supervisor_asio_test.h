#pragma once

#include "rotor/asio/supervisor_asio.h"

namespace rotor {
namespace test {

struct supervisor_asio_test_t : public rotor::asio::supervisor_asio_t{
    using rotor::asio::supervisor_asio_t::supervisor_asio_t;

    state_t& get_state() noexcept { return  state; }
    queue_t& get_queue() noexcept { return outbound; }
    subscription_points_t& get_points() noexcept { return points; }
    subscription_map_t& get_subscription() noexcept { return subscription_map; }
};


} // namespace test
} // namespace rotor
