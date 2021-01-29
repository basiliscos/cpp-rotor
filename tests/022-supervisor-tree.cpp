//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "supervisor_test.h"
#include "access.h"

namespace r = rotor;
namespace rt = r::test;

static std::uint32_t ping_received = 0;
static std::uint32_t ping_sent = 0;

struct ping_t {};

struct pinger_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;

    void set_ponger_addr(const r::address_ptr_t &addr) { ponger_addr = addr; }

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<rotor::plugin::registry_plugin_t>(
            [&](auto &p) { p.discover_name("ponger", ponger_addr, true).link(true); });
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        send<ping_t>(ponger_addr);
        ping_sent++;
    }

    r::address_ptr_t ponger_addr;
};

struct ponger_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<rotor::plugin::registry_plugin_t>(
            [&](auto &p) { p.register_name("ponger", get_address()); });
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&ponger_t::on_ping); });
    }

    void on_ping(r::message_t<ping_t> &) noexcept {
        ping_received++;
        do_shutdown();
    }
};

struct custom_sup : rt::supervisor_test_t {
    using rt::supervisor_test_t::supervisor_test_t;

    void on_child_init(actor_base_t *, const r::extended_error_ptr_t &ee_) noexcept override { ee = ee_; }
    r::extended_error_ptr_t ee;
};

/*
 * Let's have the following tree of supervisors
 *
 *      S_root
 *      |    |
 *     S_A1  S_B1
 *      |     |
 *     S_A2  S_B2
 *    /         \
 * pinger      ponger
 *
 * 1. Pinger should be able to send ping message to ponger. The message should
 * be processed by S_1, still it have to be delivered to ponger
 *
 * 2. Ponger should receive the message, and initiate it's own shutdown procedure
 *
 * 3. As all supervisors have the same localitiy, the S_2 supervisor should
 * receive ponger shutdown request and spawn a new ponger.
 *
 * 4. All messaging (except initialization) should happen in single do_process
 * pass
 *
 */
TEST_CASE("supervisor/locality tree ", "[supervisor]") {
    r::system_context_t system_context;
    const void *locality = &system_context;

    auto sup_root = system_context.create_supervisor<rt::supervisor_test_t>()
                        .locality(locality)
                        .timeout(rt::default_timeout)
                        .create_registry()
                        .finish();
    auto sup_A1 =
        sup_root->create_actor<rt::supervisor_test_t>().locality(locality).timeout(rt::default_timeout).finish();
    auto sup_A2 =
        sup_A1->create_actor<rt::supervisor_test_t>().locality(locality).timeout(rt::default_timeout).finish();

    auto sup_B1 =
        sup_root->create_actor<rt::supervisor_test_t>().locality(locality).timeout(rt::default_timeout).finish();
    auto sup_B2 =
        sup_B1->create_actor<rt::supervisor_test_t>().locality(locality).timeout(rt::default_timeout).finish();

    auto pinger = sup_A2->create_actor<pinger_t>().timeout(rt::default_timeout).finish();
    auto ponger = sup_B2->create_actor<ponger_t>().timeout(rt::default_timeout).finish();

    pinger->set_ponger_addr(ponger->get_address());
    sup_A2->do_process();

    CHECK(sup_A2->get_children_count() == 1);
    CHECK(sup_B2->get_children_count() == 1);
    CHECK(ping_sent == 1);
    CHECK(ping_received == 1);

    sup_root->do_shutdown();
    sup_root->do_process();

    REQUIRE(sup_A2->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup_B2->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup_A1->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup_B1->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup_root->get_state() == r::state_t::SHUT_DOWN);
}

TEST_CASE("failure escalation") {
    r::system_context_t system_context;
    auto sup_root =
        system_context.create_supervisor<custom_sup>().timeout(rt::default_timeout).create_registry().finish();
    auto sup_child = sup_root->create_actor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    r::address_ptr_t dummy_addr;
    auto act = sup_child->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    act->configurer = [&](auto &, r::plugin::plugin_base_t &plugin) {
        plugin.with_casted<r::plugin::registry_plugin_t>([&](auto &p) { p.discover_name("service-name", dummy_addr); });
    };
    sup_root->do_process();

    CHECK(act->get_state() == r::state_t::SHUT_DOWN);
    CHECK(sup_child->get_state() == r::state_t::SHUT_DOWN);
    CHECK(sup_root->get_state() == r::state_t::SHUT_DOWN);
    auto &ee = sup_root->ee;
    REQUIRE(ee);
    CHECK(ee->ec.message() == "failure escalation");
}
