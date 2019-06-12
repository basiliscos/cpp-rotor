#pragma once

#pragma once

#include "rotor/actor_base.h"

namespace rotor {
namespace test {

struct actor_test_t : public actor_base_t {

    using actor_base_t::actor_base_t;

    state_t& get_state() noexcept { return  state; }
    subscription_points_t& get_points() noexcept { return points; }
};

}
}
