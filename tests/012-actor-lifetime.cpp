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

static std::uint32_t destroyed = 0;

struct sample_actor_t : public r::actor_base_t {
    std::uint32_t event_current;
    std::uint32_t event_init;
    std::uint32_t event_start;
    std::uint32_t event_shutdown;
    std::uint32_t event_shutingdown;

    r::state_t &get_state() noexcept { return state; }

    sample_actor_t(r::supervisor_t &sup) : r::actor_base_t{sup} {
        event_current = 1;
        event_init = 0;
        event_start = 0;
        event_shutdown = 0;
        event_shutingdown = 0;
    }
    ~sample_actor_t() override { ++destroyed; }

    void on_initialize(r::message::init_request_t &msg) noexcept override {
        event_init = event_current++;
        r::actor_base_t::on_initialize(msg);
    }

    void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        event_start = event_current++;
        r::actor_base_t::on_start(msg);
    }

    void shutdown_start() noexcept override {
        r::actor_base_t::shutdown_start();
        if (state == r::state_t::SHUTTING_DOWN) {
            event_shutingdown = event_current++;
        }
    }

    void shutdown_finish() noexcept override {
        if (state == r::state_t::SHUTTED_DOWN) {
            event_shutdown = event_current++;
        }
        r::actor_base_t::shutdown_finish();
    }
};

struct fail_shutdown_actor : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;

    bool allow_shutdown = false;

    void shutdown_start() noexcept override {
        if (allow_shutdown) {
            r::actor_base_t::shutdown_start();
        }
    }
};

struct double_shutdown_actor : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;

    std::uint32_t shutdown_starts = 0;

    void shutdown_start() noexcept override { ++shutdown_starts; }

    void continue_shutdown() noexcept { r::actor_base_t::shutdown_start(); }
};

struct fail_initialize_actor : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;

    void init_start() noexcept override {}
};

struct fail_test_behavior_t : public r::supervisor_behavior_t {
    using r::supervisor_behavior_t::supervisor_behavior_t;

    void on_shutdown_fail(const r::address_ptr_t &address, const std::error_code &ec) noexcept override;
};

struct fail_shutdown_sup : public rt::supervisor_test_t {
    using rt::supervisor_test_t::supervisor_test_t;

    r::address_ptr_t fail_addr;
    std::error_code fail_reason;

    virtual r::actor_behavior_t *create_behavior() noexcept override { return new fail_test_behavior_t(*this); }
};

void fail_test_behavior_t::on_shutdown_fail(const r::address_ptr_t &address, const std::error_code &ec) noexcept {
    auto &sup = static_cast<fail_shutdown_sup &>(actor);
    sup.fail_addr = address;
    sup.fail_reason = ec;
}

TEST_CASE("actor litetimes", "[actor]") {
    r::system_context_t system_context;
    rt::supervisor_config_test_t config(r::pt::milliseconds{1}, nullptr);
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>(nullptr, config);
    auto act = sup->create_actor<sample_actor_t>(r::pt::milliseconds{1});

    REQUIRE(act->get_state() == r::state_t::INITIALIZING);

    sup->do_process();
    REQUIRE(act->get_state() == r::state_t::OPERATIONAL);

    auto actor_addr = act->get_address();
    act->send<r::payload::shutdown_trigger_t>(actor_addr, actor_addr);
    sup->do_process();
    REQUIRE(act->event_current == 5);
    REQUIRE(act->event_shutdown == 4);
    REQUIRE(act->event_shutingdown == 3);
    REQUIRE(act->event_start == 2);
    REQUIRE(act->event_init == 1);

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
    REQUIRE(sup->get_subscription().size() == 0);
}

TEST_CASE("fail shutdown test", "[actor]") {
    r::system_context_t system_context;

    rt::supervisor_config_test_t config(r::pt::milliseconds{1}, nullptr);
    auto sup = system_context.create_supervisor<fail_shutdown_sup>(nullptr, config);
    auto act = sup->create_actor<fail_shutdown_actor>(r::pt::milliseconds{1});

    sup->do_process();

    act->do_shutdown();
    sup->do_process();
    REQUIRE(sup->active_timers.size() == 1);

    sup->on_timer_trigger(*sup->active_timers.begin());
    sup->do_process();

    REQUIRE(sup->get_children().size() == 1);
    REQUIRE(sup->fail_addr == act->get_address());
    REQUIRE(sup->fail_reason.value() == static_cast<int>(r::error_code_t::request_timeout));

    act->allow_shutdown = true;
    act->do_shutdown();
    sup->do_process();
    REQUIRE(sup->get_children().size() == 0);

    sup->do_shutdown();
    sup->do_process();
}

TEST_CASE("fail initialize test", "[actor]") {
    r::system_context_t system_context;

    auto timeout = r::pt::millisec{1};
    rt::supervisor_config_test_t config(timeout, nullptr);
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>(nullptr, config);
    sup->do_process();

    auto act = sup->create_actor<fail_initialize_actor>(timeout);
    sup->do_process();

    REQUIRE(sup->get_children().size() == 1);
    REQUIRE(act->get_state() == r::state_t::INITIALIZING);
    REQUIRE(sup->active_timers.size() == 1);

    sup->on_timer_trigger(*sup->active_timers.begin());
    sup->do_process();
    REQUIRE(sup->get_children().size() == 0);

    sup->do_shutdown();
    sup->do_process();
}

TEST_CASE("double shutdown test (actor)", "[actor]") {
    r::system_context_t system_context;

    rt::supervisor_config_test_t config(r::pt::milliseconds{1}, nullptr);
    auto sup = system_context.create_supervisor<fail_shutdown_sup>(nullptr, config);
    auto act = sup->create_actor<double_shutdown_actor>(r::pt::milliseconds{1});

    sup->do_process();

    act->do_shutdown();
    act->do_shutdown();
    sup->do_process();
    REQUIRE(sup->active_timers.size() == 1);
    REQUIRE(act->shutdown_starts == 1);

    act->continue_shutdown();
    sup->do_process();

    REQUIRE(sup->get_children().size() == 0);

    sup->do_shutdown();
    sup->do_process();
}

TEST_CASE("double shutdown test (supervisor)", "[actor]") {
    r::system_context_t system_context;

    rt::supervisor_config_test_t config(r::pt::milliseconds{1}, nullptr);
    auto sup = system_context.create_supervisor<fail_shutdown_sup>(nullptr, config);
    auto act = sup->create_actor<double_shutdown_actor>(r::pt::milliseconds{1});

    sup->do_process();

    sup->do_shutdown();
    sup->do_shutdown();

    sup->do_process();
    REQUIRE(sup->active_timers.size() == 1);
    REQUIRE(act->shutdown_starts == 1);

    act->continue_shutdown();
    sup->do_process();

    REQUIRE(sup->get_children().size() == 0);

    sup->do_shutdown();
    sup->do_process();
}
