//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "supervisor_test.h"
#include "actor_test.h"

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

    void on_start() noexcept override{
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

struct custom_child_manager_t: public r::internal::child_manager_plugin_t {
    r::address_ptr_t fail_addr;
    std::error_code fail_ec;
    void on_shutdown_fail(r::actor_base_t &actor, const std::error_code &ec) noexcept {
        fail_addr = actor.get_address();
        fail_ec = ec;
    }
};

struct custom_supervisor_t: rt::supervisor_test_t {
    using rt::supervisor_test_t::supervisor_test_t;
    using plugins_list_t = std::tuple<
        r::internal::locality_plugin_t,
        r::internal::address_maker_plugin_t,
        r::internal::lifetime_plugin_t,
        r::internal::init_shutdown_plugin_t,
        r::internal::foreigners_support_plugin_t,
        custom_child_manager_t,
        r::internal::starter_plugin_t
    >;
};

struct fail_plugin_t: public r::plugin_t {
    static bool allow_init;
    bool allow_shutdown = true;

    static const void* class_identity;
    const void* identity() const noexcept override {
        return class_identity;
    }

    void activate(r::actor_base_t* actor_) noexcept override {
        actor_->install_plugin(*this, r::slot_t::INIT);
        actor_->install_plugin(*this, r::slot_t::SHUTDOWN);
        return r::plugin_t::activate(actor_);
    }

    bool handle_init(r::message::init_request_t* ) noexcept override {
        return allow_init;
    }

    bool handle_shutdown(r::message::shutdown_request_t* ) noexcept override {
        return allow_shutdown;
    }
};

bool fail_plugin_t::allow_init = true;

const void* fail_plugin_t::class_identity = static_cast<const void *>(typeid(fail_plugin_t).name());

struct fail_actor_t: public rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;
    using plugins_list_t = std::tuple<
        r::internal::address_maker_plugin_t,
        r::internal::lifetime_plugin_t,
        r::internal::init_shutdown_plugin_t,
        fail_plugin_t,
        r::internal::starter_plugin_t
    >;
};

TEST_CASE("actor litetimes", "[actor]") {
    r::system_context_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();

    REQUIRE(act->get_state() == r::state_t::INITIALIZING);

    sup->do_process();
    REQUIRE(act->get_state() == r::state_t::OPERATIONAL);

    auto actor_addr = act->get_address();
    act->do_shutdown();
    sup->do_process();
    CHECK(act->event_current == 6);
    CHECK(act->event_shutdown_finish == 5);
    CHECK(act->event_shutdown_start == 4);
    CHECK(act->event_start == 3);
    CHECK(act->event_init_finish == 2);
    CHECK(act->event_init_start == 1);

    REQUIRE(destroyed == 0);
    REQUIRE(act->get_state() == r::state_t::SHUTTED_DOWN);
    act.reset();
    REQUIRE(destroyed == 1);

    /* for asan */
    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().empty());
}

TEST_CASE("fail shutdown test", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<custom_supervisor_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<fail_actor_t>().timeout(rt::default_timeout).finish();

    auto fail_plugin = static_cast<fail_plugin_t*>(act->get_plugin(fail_plugin_t::class_identity));
    fail_plugin->allow_init = true;
    fail_plugin->allow_shutdown = false;

    sup->do_process();
    REQUIRE(sup->active_timers.size() == 0);

    act->do_shutdown();
    sup->do_process();
    REQUIRE(sup->active_timers.size() == 1); // "init child + shutdown children"

    sup->on_timer_trigger(*sup->active_timers.begin());
    sup->do_process();

    REQUIRE(sup->get_children().size() == 1);
    CHECK(act->get_state() == r::state_t::SHUTTING_DOWN);

    auto cm_plugin = static_cast<custom_child_manager_t*>(sup->get_plugin(custom_child_manager_t::class_identity));

    REQUIRE(cm_plugin->fail_addr == act->get_address());
    REQUIRE(cm_plugin->fail_ec.value() == static_cast<int>(r::error_code_t::request_timeout));

    fail_plugin->allow_shutdown = true;
    act->shutdown_continue();

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->get_children().size() == 0);

    CHECK(act->get_state() == r::state_t::SHUTTED_DOWN);
    CHECK(sup->get_state() == r::state_t::SHUTTED_DOWN);
    CHECK(sup->get_leader_queue().size() == 0);
    CHECK(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().empty());
}

TEST_CASE("fail initialize test", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<custom_supervisor_t>().timeout(rt::default_timeout).finish();
    sup->do_process();

    fail_plugin_t::allow_init = false;
    auto act = sup->create_actor<fail_actor_t>().timeout(rt::default_timeout).finish();
    sup->do_process();

    REQUIRE(sup->get_children().size() == 2); // sup + actor
    REQUIRE(act->get_state() == r::state_t::INITIALIZING);
    REQUIRE(sup->active_timers.size() == 1);

    sup->on_timer_trigger(*sup->active_timers.begin());
    sup->do_process();
    REQUIRE(sup->get_children().size() == 1); // just sup

    sup->do_shutdown();
    sup->do_process();
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

    REQUIRE(sup->get_children().size() == 1); // just sup

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

    REQUIRE(sup->get_children().size() == 0);
    REQUIRE(sup->active_timers.size() == 0);
}
