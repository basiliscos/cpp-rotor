//
// Copyright (c) 2019-2023 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor.hpp"
#include "supervisor_test.h"
#include "actor_test.h"
#include "access.h"

namespace r = rotor;
namespace rt = r::test;

static std::uint32_t destroyed = 0;

struct sample_actor_t : public r::actor_base_t {
    std::uint32_t event_current = 1;
    std::uint32_t event_init_start = 0;
    std::uint32_t event_init_finish = 0;
    std::uint32_t event_start = 0;
    std::uint32_t event_shutdown_start = 0;
    std::uint32_t event_shutdown_finish = 0;

    r::state_t &get_state() noexcept { return state; }

    using r::actor_base_t::actor_base_t;

    ~sample_actor_t() override { ++destroyed; }

    void init_start() noexcept override {
        event_init_start = event_current++;
        r::actor_base_t::init_start();
    }

    void init_finish() noexcept override {
        event_init_finish = event_current++;
        r::actor_base_t::init_finish();
    }

    void on_start() noexcept override {
        event_start = event_current++;
        r::actor_base_t::on_start();
    }

    void shutdown_start() noexcept override {
        event_shutdown_start = event_current++;
        r::actor_base_t::shutdown_start();
    }

    void shutdown_finish() noexcept override {
        event_shutdown_finish = event_current++;
        r::actor_base_t::shutdown_finish();
    }
};

struct custom_child_manager_t : public r::plugin::child_manager_plugin_t {
    r::address_ptr_t fail_addr;
    r::extended_error_ptr_t fail_ec;
    void on_shutdown_fail(r::actor_base_t &actor_, const r::extended_error_ptr_t &ec) noexcept override {
        fail_addr = actor_.get_address();
        fail_ec = ec;
    }
};

struct custom_supervisor_t : rt::supervisor_test_t {
    using rt::supervisor_test_t::supervisor_test_t;
    using plugins_list_t =
        std::tuple<r::plugin::address_maker_plugin_t, r::plugin::locality_plugin_t,
                   r::plugin::delivery_plugin_t<r::plugin::local_delivery_t>, r::plugin::lifetime_plugin_t,
                   r::plugin::init_shutdown_plugin_t, r::plugin::foreigners_support_plugin_t, custom_child_manager_t,
                   r::plugin::starter_plugin_t>;
};

struct fail_plugin_t : public r::plugin::plugin_base_t {
    static bool allow_init;
    bool allow_shutdown = true;

    static std::type_index class_id;

    const std::type_index &identity() const noexcept override { return class_id; }

    void activate(r::actor_base_t *actor_) noexcept override {
        reaction_on(reaction_t::INIT);
        reaction_on(reaction_t::SHUTDOWN);
        return r::plugin::plugin_base_t::activate(actor_);
    }

    bool handle_init(r::message::init_request_t *) noexcept override { return allow_init; }

    bool handle_shutdown(r::message::shutdown_request_t *) noexcept override { return allow_shutdown; }
};

std::type_index fail_plugin_t::class_id = typeid(fail_plugin_t);

TEST_CASE("actor lifetimes", "[actor]") {
    r::system_context_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();

    REQUIRE(act->get_state() == r::state_t::INITIALIZING);

    sup->do_process();
    REQUIRE(act->get_state() == r::state_t::OPERATIONAL);

    act->do_shutdown();
    sup->do_process();
    CHECK(act->event_current == 6);
    CHECK(act->event_shutdown_finish == 5);
    CHECK(act->event_shutdown_start == 4);
    CHECK(act->event_start == 3);
    CHECK(act->event_init_finish == 2);
    CHECK(act->event_init_start == 1);

    REQUIRE(destroyed == 0);
    REQUIRE(act->get_state() == r::state_t::SHUT_DOWN);
    act.reset();
    REQUIRE(destroyed == 1);

    /* for asan */
    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    CHECK(rt::empty(sup->get_subscription()));
}

TEST_CASE("fail shutdown test", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<custom_supervisor_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(sup->active_timers.size() == 0);

    act->access<rt::to::resources>()->acquire();
    act->do_shutdown();
    sup->do_process();
    REQUIRE(sup->active_timers.size() == 1); // "init child + shutdown children"

    auto timer_it = *sup->active_timers.begin();
    sup->do_invoke_timer(timer_it->request_id);
    sup->do_process();

    REQUIRE(sup->get_children_count() == 1);
    CHECK(act->get_state() == r::state_t::SHUTTING_DOWN);

    auto id = &std::as_const(custom_child_manager_t::class_identity);
    auto plugin = static_cast<r::actor_base_t &>(*sup).access<rt::to::get_plugin>(id);
    auto cm_plugin = static_cast<custom_child_manager_t *>(plugin);

    REQUIRE(cm_plugin->fail_addr == act->get_address());
    auto &ec = cm_plugin->fail_ec;
    REQUIRE(ec);
    CHECK(ec->ec.value() == static_cast<int>(r::error_code_t::request_timeout));
    REQUIRE(!ec->next);

    act->access<rt::to::resources>()->release();
    act->shutdown_continue();

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->get_children_count() == 0);

    CHECK(act->get_state() == r::state_t::SHUT_DOWN);
    CHECK(sup->get_state() == r::state_t::SHUT_DOWN);
    CHECK(sup->get_leader_queue().size() == 0);
    CHECK(sup->get_points().size() == 0);
    CHECK(rt::empty(sup->get_subscription()));
}

TEST_CASE("fail initialize test", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<custom_supervisor_t>().timeout(rt::default_timeout).finish();
    sup->do_process();

    auto act = sup->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    act->access<rt::to::resources>()->acquire();
    CHECK(act->access<rt::to::resources>()->has() == 1);
    sup->do_process();

    REQUIRE(sup->get_children_count() == 2); // sup + actor
    REQUIRE(act->get_state() == r::state_t::INITIALIZING);
    REQUIRE(sup->active_timers.size() == 1);

    auto timer_it = *sup->active_timers.begin();
    sup->do_invoke_timer(timer_it->request_id);
    act->access<rt::to::resources>()->release();
    sup->do_process();
    REQUIRE(sup->get_children_count() == 1); // just sup

    sup->do_shutdown();
    sup->do_process();
    CHECK(act->get_state() == r::state_t::SHUT_DOWN);
    CHECK(sup->get_state() == r::state_t::SHUT_DOWN);
}

TEST_CASE("double shutdown test (actor)", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();

    sup->do_process();

    act->do_shutdown();
    act->do_shutdown();
    sup->do_process();

    CHECK(act->event_current == 6);
    CHECK(act->event_shutdown_finish == 5);
    CHECK(act->event_shutdown_start == 4);

    REQUIRE(sup->get_children_count() == 1); // just sup

    sup->do_shutdown();
    sup->do_process();
}

TEST_CASE("double shutdown test (supervisor)", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    /* auto act = */ sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();

    sup->do_process();

    sup->do_shutdown();
    sup->do_shutdown();
    sup->do_process();

    REQUIRE(sup->get_children_count() == 0);
    REQUIRE(sup->active_timers.size() == 0);
}

TEST_CASE("autoshutdown supervisor", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).autoshutdown_supervisor().finish();

    sup->do_process();

    CHECK(act->get_state() == r::state_t::OPERATIONAL);
    CHECK(sup->get_state() == r::state_t::OPERATIONAL);

    act->do_shutdown();
    sup->do_process();

    CHECK(act->get_state() == r::state_t::SHUT_DOWN);
    CHECK(sup->get_state() == r::state_t::SHUT_DOWN);
}

TEST_CASE("escalate failure", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).escalate_failure().finish();

    sup->do_process();

    CHECK(act->get_state() == r::state_t::OPERATIONAL);
    CHECK(sup->get_state() == r::state_t::OPERATIONAL);

    auto ec = r::make_error_code(r::error_code_t::cancelled);
    auto ee = r::make_error("ctx", ec);
    act->do_shutdown(ee);
    sup->do_process();

    CHECK(act->get_state() == r::state_t::SHUT_DOWN);
    CHECK(sup->get_state() == r::state_t::SHUT_DOWN);
}
