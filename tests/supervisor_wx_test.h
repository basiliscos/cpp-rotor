#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/wx.hpp"
#include "access.h"

namespace rotor {
namespace test {

class RotorApp : public wxAppConsole {
  public:
    virtual ~RotorApp() override {}
};

struct supervisor_wx_test_t : public rotor::wx::supervisor_wx_t {
    using rotor::wx::supervisor_wx_t::supervisor_wx_t;

    state_t &get_state() noexcept { return state; }
    auto &get_leader_queue() { return access<to::locality_leader>()->access<to::queue>(); }
    subscription_t &get_subscription() noexcept { return subscription_map; }
};

} // namespace test
} // namespace rotor
