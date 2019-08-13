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

    r::state_t& get_state() noexcept { return  state; }

    sample_actor_t(r::supervisor_t &sup): r::actor_base_t{sup} {
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

    void on_shutdown(r::message_t<r::payload::shutdown_request_t> &msg) noexcept override {
        event_shutdown = event_current++;
        r::actor_base_t::on_shutdown(msg);
        if (state == r::state_t::SHUTTING_DOWN) ++event_shutingdown;
    }
};


TEST_CASE("actor litetimes", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>(nullptr, nullptr);
    auto act = sup->create_actor<sample_actor_t>();

    REQUIRE(act->get_state() == r::state_t::INITIALIZING);

    sup->do_start();
    sup->do_process();
    REQUIRE(act->get_state() == r::state_t::OPERATIONAL);

    act->do_shutdown();
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
