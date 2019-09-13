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

struct foo_t {};

struct simpleton_actor_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;

    void init_start() noexcept override {
        subscribe(&simpleton_actor_t::on_foo);
        r::actor_base_t::init_start();
    }

    void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        r::actor_base_t::on_start(msg);
        INFO("simpleton_actor_t::on_start()")
        send<foo_t>(address);
    }

    void on_foo(r::message_t<foo_t> &) noexcept {
        INFO("simpleton_actor_t::on_foo()")
        ++foo_count;
    }

    std::uint32_t foo_count = 0;
};

struct foo_observer_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;

    void set_simpleton(r::address_ptr_t addr) { simpleton_addr = std::move(addr); }

    void init_start() noexcept override {
        subscribe(&foo_observer_t::on_foo, simpleton_addr);
        r::actor_base_t::init_start();
    }

    void on_foo(r::message_t<foo_t> &) noexcept {
        ++foo_count;
        INFO("foo_observer_t::on_foo")
    }

    r::address_ptr_t simpleton_addr;
    std::uint32_t foo_count = 0;
};

TEST_CASE("obsrever", "[actor]") {
    r::system_context_t system_context;

    auto timeout = r::pt::milliseconds{1};
    rt::supervisor_config_test_t config(timeout, nullptr);
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>(nullptr, config);
    auto simpleton = sup->create_actor<simpleton_actor_t>(timeout);
    auto observer = sup->create_actor<foo_observer_t>(timeout);
    observer->set_simpleton(simpleton->get_address());

    sup->do_process();

    REQUIRE(simpleton->foo_count == 1);
    REQUIRE(observer->foo_count == 1);

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().size() == 0);
}
