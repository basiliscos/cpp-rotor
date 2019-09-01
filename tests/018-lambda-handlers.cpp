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

struct payload {};

struct sample_actor_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;
    bool received = false;

    void on_initialize(r::message::init_request_t &msg) noexcept override {
        using message_t = r::message_t<payload>;
        r::actor_base_t::on_initialize(msg);
        subscribe(r::lambda<message_t>([this](message_t &) noexcept { received = true; }));
    }

    void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        r::actor_base_t::on_start(msg);
        send<payload>(address);
    }
};

TEST_CASE("lambda handler", "[actor]") {
    r::system_context_t system_context;

    auto timeout = r::pt::milliseconds{1};
    rt::supervisor_config_test_t config(timeout, nullptr);
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>(nullptr, config);
    auto actor = sup->create_actor<sample_actor_t>(timeout);
    sup->do_process();

    REQUIRE(sup->active_timers.size() == 0);
    REQUIRE(actor->received == true);

    sup->do_shutdown();
    sup->do_process();

    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().size() == 0);
}
