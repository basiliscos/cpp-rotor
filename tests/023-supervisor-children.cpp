//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "actor_test.h"
#include "supervisor_test.h"
#include "system_context_test.h"

namespace r = rotor;
namespace rt = r::test;

struct sample_actor_t : public rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;

    virtual void init_finish() noexcept override {}

     void confirm_init() noexcept { r::actor_base_t::init_finish(); }
};

struct hello_actor_t : public rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;

    void on_start() noexcept override {
        rt::actor_test_t::on_start();
        supervisor->do_shutdown();
    }
};

struct sample_supervisor_t : public rt::supervisor_test_t {
    using rt::supervisor_test_t::supervisor_test_t;
    using child_ptr_t = r::intrusive_ptr_t<sample_actor_t>;

    void configure(r::plugin_t& plugin) noexcept override {
        plugin.with_casted<r::internal::child_manager_plugin_t>([this](auto&){
            child = create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();

        });
    }

    child_ptr_t child;
};

TEST_CASE("supervisor is not initialized, while it child did not confirmed initialization", "[supervisor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act->get_state() == r::state_t::INITIALIZING);

    act->confirm_init();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(act->get_state() == r::state_t::OPERATIONAL);

    sup->do_shutdown();
    sup->do_process();
}

TEST_CASE("supervisor is not initialized, while it 1 of 2 children did not confirmed initialization", "[supervisor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act1 = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();
    auto act2 = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act1->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act2->get_state() == r::state_t::INITIALIZING);
    act1->confirm_init();

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act1->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(act2->get_state() == r::state_t::INITIALIZING);

    sup->do_shutdown();
    sup->do_process();
}

TEST_CASE("supervisor does not starts, if a children did not initialized", "[supervisor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act1 = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();
    auto act2 = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act1->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act2->get_state() == r::state_t::INITIALIZING);
    act1->confirm_init();
    sup->do_process();
    REQUIRE(act1->get_state() == r::state_t::OPERATIONAL);

    REQUIRE(sup->active_timers.size() == 2);
    auto act2_init_req = sup->get_timer(1);
    sup->on_timer_trigger(act2_init_req);
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(act1->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(act2->get_state() == r::state_t::SHUTTED_DOWN);
}

TEST_CASE("supervisor create child during init phase", "[supervisor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<sample_supervisor_t>().timeout(rt::default_timeout).finish();
    auto &act = sup->child;

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act->get_state() == r::state_t::INITIALIZING);

    act->confirm_init();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(act->get_state() == r::state_t::OPERATIONAL);

    sup->do_shutdown();
    sup->do_process();
}

#if 0
TEST_CASE("shutdown_failed policy", "[supervisor]") {
    r::system_context_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>()
                   .policy(r::supervisor_policy_t::shutdown_failed)
                   .timeout(rt::default_timeout)
                   .finish();
    auto act = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act->get_state() == r::state_t::INITIALIZING);

    auto act_init_req = sup->get_timer(1);
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

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>()
                   .policy(r::supervisor_policy_t::shutdown_self)
                   .timeout(rt::default_timeout)
                   .finish();
    auto act = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act->get_state() == r::state_t::INITIALIZING);

    auto act_init_req = sup->get_timer(1);
    sup->on_timer_trigger(act_init_req);
    sup->do_process();

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(act->get_state() == r::state_t::SHUTTED_DOWN);
}

TEST_CASE("shutdown supervisor during actor start", "[supervisor]") {
    r::system_context_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<hello_actor_t>().timeout(rt::default_timeout).finish();

    sup->do_process();
    REQUIRE(act->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
}

TEST_CASE("misconfigured actor", "[supervisor]") {
    rt::system_context_test_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();

    auto act = sup->create_actor<hello_actor_t>().finish();

    REQUIRE(!act);
    REQUIRE(system_context.ec.value() == static_cast<int>(r::error_code_t::actor_misconfigured));
    REQUIRE(sup->get_children().empty());

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
}
#endif
