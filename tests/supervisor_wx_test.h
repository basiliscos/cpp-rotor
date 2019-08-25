#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/wx.hpp"

namespace rotor {
namespace test {

class RotorApp : public wxAppConsole {
  public:
    virtual ~RotorApp() override {}
};

struct supervisor_wx_test_t : public rotor::wx::supervisor_wx_t {
    using rotor::wx::supervisor_wx_t::supervisor_wx_t;

    state_t &get_state() noexcept { return state; }
    queue_t &get_queue() noexcept { return *effective_queue; }
    subscription_points_t &get_points() noexcept { return points; }
    subscription_map_t &get_subscription() noexcept { return subscription_map; }
};

} // namespace test
} // namespace rotor
