//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
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

    void configure(r::plugin_t& plugin) noexcept override {
        if (plugin.identity() == r::internal::initializer_plugin_t::class_identity) {
            auto p = static_cast<r::internal::initializer_plugin_t&>(plugin);
            p.subscribe_actor(&simpleton_actor_t::on_foo);
        }
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
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

    void configure(r::plugin_t& plugin) noexcept override {
        if (plugin.identity() == r::internal::initializer_plugin_t::class_identity) {
            auto p = static_cast<r::internal::initializer_plugin_t&>(plugin);
            p.subscribe_actor(&foo_observer_t::on_foo, simpleton_addr);
        }
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

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto simpleton = sup->create_actor<simpleton_actor_t>().timeout(rt::default_timeout).finish();
    auto observer = sup->create_actor<foo_observer_t>().timeout(rt::default_timeout).finish();
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
