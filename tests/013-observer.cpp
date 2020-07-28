//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "supervisor_test.h"
#include "access.h"

namespace r = rotor;
namespace rt = r::test;

struct foo_t {};

struct simpleton_actor_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&simpleton_actor_t::on_foo); });
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

struct observer_config_t : r::actor_config_t {
    r::address_ptr_t observable;
    using r::actor_config_t::actor_config_t;
};

template <typename Actor> struct observer_config_builder_t : r::actor_config_builder_t<Actor> {
    using builder_t = typename Actor::template config_builder_t<Actor>;
    using parent_t = r::actor_config_builder_t<Actor>;
    using parent_t::parent_t;

    builder_t &&observable(const r::address_ptr_t &addr) {
        parent_t::config.observable = addr;
        return std::move(*static_cast<builder_t *>(this));
    }

    bool validate() noexcept override { return parent_t::config.observable && parent_t::validate(); }
};

struct foo_observer_t : public r::actor_base_t {
    using config_t = observer_config_t;
    template <typename Actor> using config_builder_t = observer_config_builder_t<Actor>;

    explicit foo_observer_t(config_t &cfg) : r::actor_base_t(cfg), simpleton_addr{cfg.observable} {}

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<r::plugin::starter_plugin_t>(
            [this](auto &p) { p.subscribe_actor(&foo_observer_t::on_foo, simpleton_addr); });
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
    auto &simpleton_addr = simpleton->get_address();
    auto observer =
        sup->create_actor<foo_observer_t>().observable(simpleton_addr).timeout(rt::default_timeout).finish();
    sup->do_process();

    REQUIRE(simpleton->foo_count == 1);
    REQUIRE(observer->foo_count == 1);

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(rt::empty(sup->get_subscription()));
}
