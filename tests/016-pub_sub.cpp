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

struct payload_t {};

struct pub_config_t: r::actor_config_t {
    r::address_ptr_t pub_addr;
    using r::actor_config_t::actor_config_t;
};

template <typename Actor> struct pub_config_builder_t: r::actor_config_builder_t<Actor> {
    using builder_t = typename Actor::template config_builder_t<Actor>;
    using parent_t = r::actor_config_builder_t<Actor>;
    using parent_t::parent_t;


    builder_t&& pub_addr(const r::address_ptr_t& addr) {
        parent_t::config.pub_addr = addr;
        return std::move(*static_cast<builder_t *>(this));
    }

    bool validate() noexcept override { return parent_t::config.pub_addr && parent_t::validate(); }
};


struct pub_t : public r::actor_base_t {
    using config_t = pub_config_t;
    template <typename Actor> using config_builder_t = pub_config_builder_t<Actor>;

    explicit pub_t (config_t& cfg): r::actor_base_t(cfg), pub_addr{cfg.pub_addr} {}

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        send<payload_t>(pub_addr);
    }

    r::address_ptr_t pub_addr;
};

struct sub_t : public r::actor_base_t {
    using config_t = pub_config_t;
    template <typename Actor> using config_builder_t = pub_config_builder_t<Actor>;

    explicit sub_t (config_t& cfg): r::actor_base_t(cfg), pub_addr{cfg.pub_addr} {}

    void configure(r::plugin_t& plugin) noexcept override {
        plugin.with_casted<r::internal::starter_plugin_t>([this](auto& p){
            p.subscribe_actor(&sub_t::on_payload, pub_addr);
        });
    }

    void on_payload(r::message_t<payload_t> &) noexcept { ++received; }

    std::uint16_t received = 0;
    r::address_ptr_t pub_addr;
};

TEST_CASE("ping-pong", "[supervisor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto pub_addr = sup->create_address();
    auto pub = sup->create_actor<pub_t>().pub_addr(pub_addr).timeout(rt::default_timeout).finish();
    auto sub1 = sup->create_actor<sub_t>().pub_addr(pub_addr).timeout(rt::default_timeout).finish();
    auto sub2 = sup->create_actor<sub_t>().pub_addr(pub_addr).timeout(rt::default_timeout).finish();

    sup->do_process();

    REQUIRE(sub1->received == 1);
    REQUIRE(sub2->received == 1);

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().empty());
}
