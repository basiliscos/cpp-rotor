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

struct spy_behaviour : public r::actor_shutdown_t {
    using r::actor_shutdown_t::actor_shutdown_t;
    virtual void cleanup() noexcept;
};

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

    void on_initialize(r::message_t<r::payload::initialize_actor_t> &msg) noexcept override {
        event_init = event_current++;
        r::actor_base_t::on_initialize(msg);
    }

    void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        event_start = event_current++;
        r::actor_base_t::on_start(msg);
    }

    void on_shutdown(r::message::shutdown_request_t &msg) noexcept override {
        event_shutdown = event_current++;
        r::actor_base_t::on_shutdown(msg);
    }

    virtual void shutdown_initiate() noexcept override {
        behaviour = std::make_unique<spy_behaviour>(*this);
        behaviour->init();
    }
};

void spy_behaviour::cleanup() noexcept {
    auto &sup = static_cast<sample_actor_t &>(actor);
    ++sup.event_shutingdown;
}

struct fail_shutdown_actor : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;

    bool allow_shutdown = false;

    virtual void shutdown_initiate() noexcept override {
        if (allow_shutdown) {
            behaviour = std::make_unique<r::actor_shutdown_t>(*this);
            behaviour->init();
        }
    }
};

struct fail_shutdown_sup : public rt::supervisor_test_t {
    using rt::supervisor_test_t::supervisor_test_t;

    r::address_ptr_t fail_addr;
    std::error_code fail_reason;

    virtual void on_fail_shutdown(const r::address_ptr_t &addr, const std::error_code &ec) noexcept {
        fail_addr = addr;
        fail_reason = ec;
    }
};

TEST_CASE("actor litetimes", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>(nullptr, r::pt::milliseconds{500}, nullptr);
    auto act = sup->create_actor<sample_actor_t>();

    REQUIRE(act->get_state() == r::state_t::INITIALIZING);

    sup->do_process();
    REQUIRE(act->get_state() == r::state_t::OPERATIONAL);

    auto actor_addr = act->get_address();
    act->send<r::payload::shutdown_trigger_t>(actor_addr, actor_addr);
    sup->do_process();
    REQUIRE(act->event_current == 4);
    REQUIRE(act->event_shutdown == 3);
    REQUIRE(act->event_shutingdown == 1);
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
    REQUIRE(sup->get_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().size() == 0);
}

TEST_CASE("fail shutdown test", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<fail_shutdown_sup>(nullptr, r::pt::milliseconds{500}, nullptr);
    auto act = sup->create_actor<fail_shutdown_actor>();

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
