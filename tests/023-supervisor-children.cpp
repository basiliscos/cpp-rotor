//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "actor_test.h"
#include "supervisor_test.h"

namespace r = rotor;
namespace rt = r::test;

struct sample_actor_t : public rt::actor_test_t {

    using rt::actor_test_t::actor_test_t;

    virtual void init_start() noexcept override {}

    virtual void confirm_init_start() noexcept {
        r::actor_base_t::init_start();
    }

};

TEST_CASE("supervisor is not initialized, while it child did not confirmed initialization", "[supervisor]") {
    r::system_context_t system_context;
    const void *locality = &system_context;

    auto timeout = r::pt::milliseconds{1};
    rt::supervisor_config_test_t config(timeout, locality);
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>(nullptr, config);
    auto act = sup->create_actor<sample_actor_t>(timeout);

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act->get_state() == r::state_t::INITIALIZING);

    act->confirm_init_start();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(act->get_state() == r::state_t::OPERATIONAL);

    sup->do_shutdown();
    sup->do_process();
}

TEST_CASE("supervisor is not initialized, while it 1 of 2 children did not confirmed initialization", "[supervisor]") {
    r::system_context_t system_context;
    const void *locality = &system_context;

    auto timeout = r::pt::milliseconds{1};
    rt::supervisor_config_test_t config(timeout, locality);
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>(nullptr, config);
    auto act1 = sup->create_actor<sample_actor_t>(timeout);
    auto act2 = sup->create_actor<sample_actor_t>(timeout);

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act1->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act2->get_state() == r::state_t::INITIALIZING);
    act1->confirm_init_start();

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act1->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(act2->get_state() == r::state_t::INITIALIZING);

    sup->do_shutdown();
    sup->do_process();
}

TEST_CASE("supervisor does not starts, if a children did not initialized", "[supervisor]") {
    r::system_context_t system_context;
    const void *locality = &system_context;

    auto timeout = r::pt::milliseconds{1};
    rt::supervisor_config_test_t config(timeout, locality);
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>(nullptr, config);
    auto act1 = sup->create_actor<sample_actor_t>(timeout);
    auto act2 = sup->create_actor<sample_actor_t>(timeout);

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act1->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act2->get_state() == r::state_t::INITIALIZING);
    act1->confirm_init_start();
    sup->do_process();
    REQUIRE(act1->get_state() == r::state_t::OPERATIONAL);

    REQUIRE(sup->active_timers.size() == 2);
    auto sup_init_req = *(sup->active_timers.begin());
    auto act2_init_req = sup_init_req;
    for(auto& r: sup->active_timers) {
        if(r > act2_init_req) {
            act2_init_req = r;
        }
    }
    sup->on_timer_trigger(act2_init_req);
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(act1->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(act2->get_state() == r::state_t::SHUTTED_DOWN);
}
