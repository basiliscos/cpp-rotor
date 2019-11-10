#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//


#include "rotor/actor_base.h"
#include "rotor/behavior.h"

namespace rotor {
namespace test {

struct test_behaviour: public actor_behavior_t {
    using actor_behavior_t::actor_behavior_t;

    linking_actors_t& get_linking_actors() noexcept { return  linking_actors;}
};

struct actor_test_t : public actor_base_t {

    using actor_base_t::actor_base_t;


    virtual actor_behavior_t *create_behavior() noexcept override {
        return new test_behaviour(*this);
    }

    state_t& get_state() noexcept { return  state; }
    subscription_points_t& get_points() noexcept { return points; }
    test_behaviour& get_behaviour() noexcept { return *static_cast<test_behaviour*>(behavior); }
    linked_clients_t& get_linked_clients() noexcept { return linked_clients; }
    linked_servers_t& get_linked_servers() noexcept { return linked_servers; }
};

}
}
