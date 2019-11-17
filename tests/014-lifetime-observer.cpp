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

struct sample_actor_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;
};

struct observer_t : public r::actor_base_t {
    std::uint32_t event = 0;
    r::address_ptr_t observable;

    using r::actor_base_t::actor_base_t;

    void set_observable(r::address_ptr_t addr) { observable = std::move(addr); }

    void on_initialize(r::message::init_request_t &msg) noexcept override {
        r::actor_base_t::on_initialize(msg);
        subscribe(&observer_t::on_sample_initialize, observable);
        subscribe(&observer_t::on_sample_start, observable);
        subscribe(&observer_t::on_sample_shutdown, observable);
    }

    void on_sample_initialize(r::message::init_request_t &) noexcept { event += 1; }

    void on_sample_start(r::message_t<r::payload::start_actor_t> &) noexcept { event += 2; }

    void on_sample_shutdown(r::message::shutdown_request_t &) noexcept { event += 4; }
};

TEST_CASE("lifetime observer, same locality", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto observer = sup->create_actor<observer_t>().timeout(rt::default_timeout).finish();
    auto sample_actor = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();
    observer->set_observable(sample_actor->get_address());

    sup->do_process();
    REQUIRE(observer->event == 3);

    sample_actor->do_shutdown();
    sup->do_process();
    REQUIRE(observer->event == 7);

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().size() == 0);
}

TEST_CASE("lifetime observer, different localities", "[actor]") {
    r::system_context_t system_context;

    const char locality1[] = "l1";
    const char locality2[] = "l2";
    auto sup1 = system_context.create_supervisor<rt::supervisor_test_t>()
                    .locality(locality1)
                    .timeout(rt::default_timeout)
                    .finish();
    auto sup2 = sup1->create_actor<rt::supervisor_test_t>().locality(locality2).timeout(rt::default_timeout).finish();
    auto observer = sup1->create_actor<observer_t>().timeout(rt::default_timeout).finish();
    auto sample_actor = sup2->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();
    observer->set_observable(sample_actor->get_address());

    sup1->do_process();
    REQUIRE(observer->event == 0);

    sup1->do_process();
    sup2->do_process();
    sup1->do_process();
    sup2->do_process();

    REQUIRE(observer->event == 3);

    sample_actor->do_shutdown();
    sup2->do_process();
    sup1->do_process();
    sup2->do_process();
    sup1->do_process();

    REQUIRE(observer->event == 7);

    sup1->do_shutdown();
    sup1->do_process();
    sup2->do_process();
    sup1->do_process();
    sup2->do_process();

    REQUIRE(sup1->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup1->get_leader_queue().size() == 0);
    REQUIRE(sup1->get_points().size() == 0);
    REQUIRE(sup1->get_subscription().size() == 0);
}
