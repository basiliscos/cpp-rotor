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

struct sample_supervisor_t: public rt::supervisor_test_t {
    using rt::supervisor_test_t::supervisor_test_t;
    using child_ptr_t = r::intrusive_ptr_t<sample_actor_t>;

    virtual void init_start() noexcept override {
        auto timeout = r::pt::milliseconds{1};
        child= create_actor<sample_actor_t>(timeout);
    }

    child_ptr_t child;
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
    sup->pop_timer();
    auto act2_init_req = sup->pop_timer();
    sup->on_timer_trigger(act2_init_req);
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(act1->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(act2->get_state() == r::state_t::SHUTTED_DOWN);
}


TEST_CASE("supervisor create child during init phase", "[supervisor]") {
    r::system_context_t system_context;
    const void *locality = &system_context;

    auto timeout = r::pt::milliseconds{1};
    rt::supervisor_config_test_t config(timeout, locality);
    auto sup = system_context.create_supervisor<sample_supervisor_t>(nullptr, config);
    auto& act = sup->child;

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

TEST_CASE("shutdown_failed policy", "[supervisor]") {
    r::system_context_t system_context;
    const void *locality = &system_context;

    auto timeout = r::pt::milliseconds{1};
    rt::supervisor_config_test_t config(timeout, locality, r::supervisor_policy_t::shutdown_failed);
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>(nullptr, config);
    auto act = sup->create_actor<sample_actor_t>(timeout);

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act->get_state() == r::state_t::INITIALIZING);

    sup->pop_timer();
    auto act_init_req = sup->pop_timer();
    sup->on_timer_trigger(act_init_req);
    sup->do_process();

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(act->get_state() == r::state_t::SHUTTED_DOWN);

    sup->do_shutdown();
    sup->do_process();
}

TEST_CASE("shutdown_self policy", "[supervisor]") {
    r::system_context_t system_context;
    const void *locality = &system_context;

    auto timeout = r::pt::milliseconds{1};
    rt::supervisor_config_test_t config(timeout, locality, r::supervisor_policy_t::shutdown_self);
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>(nullptr, config);
    auto act = sup->create_actor<sample_actor_t>(timeout);

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act->get_state() == r::state_t::INITIALIZING);

    sup->pop_timer();
    auto act_init_req = sup->pop_timer();
    sup->on_timer_trigger(act_init_req);
    sup->do_process();

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(act->get_state() == r::state_t::SHUTTED_DOWN);
}
