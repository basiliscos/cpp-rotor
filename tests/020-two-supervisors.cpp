//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "supervisor_test.h"

namespace r = rotor;
namespace rt = r::test;

struct my_supervisor_t : public rt::supervisor_test_t {
    using rt::supervisor_test_t::supervisor_test_t;

    void init_start() noexcept override {
        if (state == r::state_t::INITIALIZING) { on_initialize_count++; }
        rt::supervisor_test_t::init_start();
    }

    void shutdown_finish() noexcept override {
        rt::supervisor_test_t::shutdown_finish();
        if (state == r::state_t::SHUTTED_DOWN) { on_shutdown_count++; }
    }

    std::uint32_t on_initialize_count = 0;
    std::uint32_t on_shutdown_count = 0;
};

TEST_CASE("two supervisors, different localities", "[supervisor]") {
    r::system_context_t system_context;

    const char locality1[] = "abc";
    const char locality2[] = "def";
    auto timeout = r::pt::milliseconds{1};
    rt::supervisor_config_test_t config1(timeout, locality1);
    rt::supervisor_config_test_t config2(timeout, locality2);
    auto sup1 = system_context.create_supervisor<my_supervisor_t>(nullptr, config1);
    auto sup2 = sup1->create_actor<my_supervisor_t>(timeout, config2);

    REQUIRE(&sup2->get_supervisor() == sup2.get());
    REQUIRE(sup2->get_parent_supervisor() == sup1.get());

    sup1->do_process();
    REQUIRE(sup1->get_state() == r::state_t::INITIALIZING);
    REQUIRE(sup2->get_state() == r::state_t::INITIALIZING);
    REQUIRE(sup1->on_initialize_count == 1);
    REQUIRE(sup2->on_initialize_count == 0);

    sup2->do_process();
    REQUIRE(sup1->get_state() == r::state_t::INITIALIZING);
    REQUIRE(sup2->get_state() == r::state_t::INITIALIZED);
    REQUIRE(sup1->on_initialize_count == 1);
    REQUIRE(sup2->on_initialize_count == 1);
    REQUIRE(sup1->on_shutdown_count == 0);

    sup1->do_process();
    REQUIRE(sup1->on_initialize_count == 1);
    REQUIRE(sup2->on_initialize_count == 1);
    REQUIRE(sup1->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(sup2->get_state() == r::state_t::INITIALIZED);

    sup2->do_process();
    REQUIRE(sup2->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(sup1->on_initialize_count == 1);
    REQUIRE(sup2->on_initialize_count == 1);
    REQUIRE(sup2->on_shutdown_count == 0);
    REQUIRE(sup1->on_shutdown_count == 0);

    sup2->do_shutdown();
    sup2->do_process();
    sup1->do_process();
    sup2->do_process();

    REQUIRE(sup1->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(sup2->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup1->on_shutdown_count == 0);
    REQUIRE(sup2->on_shutdown_count == 1);

    sup1->do_process();
    REQUIRE(sup1->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(sup2->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup1->on_shutdown_count == 0);
    REQUIRE(sup2->on_shutdown_count == 1);

    sup1->do_shutdown();
    sup1->do_process();
    REQUIRE(sup1->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup1->on_shutdown_count == 1);

    REQUIRE(sup1->get_leader_queue().size() == 0);
    REQUIRE(sup1->get_points().size() == 0);
    REQUIRE(sup1->get_subscription().size() == 0);

    REQUIRE(sup2->get_leader_queue().size() == 0);
    REQUIRE(sup2->get_points().size() == 0);
    REQUIRE(sup2->get_subscription().size() == 0);
}

TEST_CASE("two supervisors, same locality", "[supervisor]") {
    r::system_context_t system_context;

    const char locality[] = "locality";
    auto timeout = r::pt::milliseconds{1};
    rt::supervisor_config_test_t config(timeout, locality);
    auto sup1 = system_context.create_supervisor<my_supervisor_t>(nullptr, config);
    auto sup2 = sup1->create_actor<my_supervisor_t>(timeout, config);

    REQUIRE(&sup2->get_supervisor() == sup2.get());
    REQUIRE(sup2->get_parent_supervisor() == sup1.get());

    sup1->do_process();
    REQUIRE(sup1->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(sup2->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(sup2->on_initialize_count == 1);

    sup1->do_shutdown();
    sup1->do_process();

    REQUIRE(sup1->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup2->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup1->on_shutdown_count == 1);

    REQUIRE(sup1->get_leader_queue().size() == 0);
    REQUIRE(sup1->get_points().size() == 0);
    REQUIRE(sup1->get_subscription().size() == 0);

    REQUIRE(sup2->get_leader_queue().size() == 0);
    REQUIRE(sup2->get_points().size() == 0);
    REQUIRE(sup2->get_subscription().size() == 0);
}

TEST_CASE("two supervisors, down internal first, same locality", "[supervisor]") {
    r::system_context_t system_context;

    const char locality[] = "locality";
    auto timeout = r::pt::milliseconds{1};
    rt::supervisor_config_test_t config(timeout, locality);
    auto sup1 = system_context.create_supervisor<my_supervisor_t>(nullptr, config);
    auto sup2 = sup1->create_actor<my_supervisor_t>(timeout, config);

    REQUIRE(&sup2->get_supervisor() == sup2.get());
    REQUIRE(sup2->get_parent_supervisor() == sup1.get());

    sup1->do_process();
    REQUIRE(sup1->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(sup2->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(sup2->on_initialize_count == 1);

    // for better coverage
    sup2->template send<r::payload::shutdown_trigger_t>(sup2->get_address(), sup2->get_address());
    sup1->do_process();

    REQUIRE(sup2->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup1->get_state() == r::state_t::OPERATIONAL);

    sup1->do_shutdown();
    sup1->do_process();
    REQUIRE(sup1->get_state() == r::state_t::SHUTTED_DOWN);

    REQUIRE(sup1->get_leader_queue().size() == 0);
    REQUIRE(sup1->get_points().size() == 0);
    REQUIRE(sup1->get_subscription().size() == 0);

    REQUIRE(sup2->get_leader_queue().size() == 0);
    REQUIRE(sup2->get_points().size() == 0);
    REQUIRE(sup2->get_subscription().size() == 0);
}
