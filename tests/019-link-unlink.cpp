//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "actor_test.h"
#include "supervisor_test.h"
#include "system_context_test.h"
#include "access.h"

namespace r = rotor;
namespace rt = r::test;

TEST_CASE("client/server, common workflow", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act_s = sup->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto act_c = sup->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto &addr_s = act_s->access<rt::to::address>();

    bool invoked = false;
    act_c->configurer = [&](auto &, r::plugin_t &plugin) {
        plugin.with_casted<r::internal::link_client_plugin_t>([&](auto &p) {
            p.link(addr_s, [&](auto &ec) mutable {
                REQUIRE(!ec);
                invoked = true;
            });
        });
    };
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(invoked);

    SECTION("simultaneous shutdown") {
        sup->do_shutdown();
        sup->do_process();
    }

    SECTION("controlled shutdown") {
        SECTION("indirect client-initiated unlink via client-shutdown") {
            act_c->do_shutdown();
            sup->do_process();
            CHECK(act_c->get_state() == r::state_t::SHUTTED_DOWN);
        }

        SECTION("indirect client-initiated unlink via server-shutdown") {
            act_s->do_shutdown();
            sup->do_process();
            CHECK(act_s->get_state() == r::state_t::SHUTTED_DOWN);
            CHECK(act_c->get_state() == r::state_t::SHUTTED_DOWN);
        }

        sup->do_shutdown();
        sup->do_process();
    }
}

TEST_CASE("link not possible (timeout) => shutdown", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act_c = sup->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto some_addr = sup->make_address();

    bool invoked = false;
    act_c->configurer = [&](auto &, r::plugin_t &plugin) {
        plugin.with_casted<r::internal::link_client_plugin_t>([&](auto &p) {
            p.link(some_addr, [&](auto &ec) mutable {
                REQUIRE(ec);
                invoked = true;
            });
        });
    };
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::INITIALIZING);

    REQUIRE(sup->active_timers.size() == 3);
    auto timer_id = *(sup->active_timers.rbegin());
    sup->on_timer_trigger(timer_id);

    sup->do_process();
    REQUIRE(invoked);
    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
}

#if 0
TEST_CASE("link not possible => supervisor is shutted down", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act_s = sup->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto act_c = sup->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();

    auto server_addr = act_s->get_address();

    act_c->link_request(server_addr, rt::default_timeout);
    sup->do_process();

    REQUIRE(act_c->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(act_s->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
}
#endif

TEST_CASE("unlink", "[actor]") {
    rt::system_context_test_t system_context;

    const char l1[] = "abc";
    const char l2[] = "def";

    auto sup1 =
        system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).locality(l1).finish();
    auto sup2 = sup1->create_actor<rt::supervisor_test_t>().timeout(rt::default_timeout).locality(l2).finish();

    auto act_s = sup1->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto act_c = sup2->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto &addr_s = act_s->access<rt::to::address>();

    act_c->configurer = [&](auto &, r::plugin_t &plugin) {
        plugin.with_casted<r::internal::link_client_plugin_t>([&](auto &p) { p.link(addr_s, [&](auto &) {}); });
    };
    while (!sup1->get_leader_queue().empty() || !sup2->get_leader_queue().empty()) {
        sup1->do_process();
        sup2->do_process();
    }
    REQUIRE(sup1->get_state() == r::state_t::OPERATIONAL);

    SECTION("unlink failure") {
        act_s->do_shutdown();
        sup1->do_process();
        REQUIRE(sup1->active_timers.size() == 2);

        auto unlink_req = sup1->get_timer(1);
        sup1->on_timer_trigger(unlink_req);
        sup1->do_process();

        REQUIRE(system_context.ec == r::error_code_t::request_timeout);
        REQUIRE(act_s->get_state() == r::state_t::SHUTTING_DOWN);
        act_s->force_cleanup();
    }

    SECTION("unlink-notify on unlink-request") {
        SECTION("client, then server") {
            act_s->do_shutdown();
            act_c->do_shutdown();
            sup1->do_process();
            sup2->do_process();
            sup1->do_process();
            sup2->do_process();
        }

        SECTION("server, then client") {
            act_s->do_shutdown();
            act_c->do_shutdown();
            sup1->do_process();
            sup2->do_process();
            sup1->do_process();
            sup2->do_process();
        }
    }

    sup1->do_shutdown();
    while (!sup1->get_leader_queue().empty() || !sup2->get_leader_queue().empty()) {
        sup1->do_process();
        sup2->do_process();
    }
    REQUIRE(sup1->get_state() == r::state_t::SHUTTED_DOWN);
}
