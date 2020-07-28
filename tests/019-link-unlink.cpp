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
    auto &addr_s = act_s->get_address();

    bool invoked = false;
    act_c->configurer = [&](auto &, r::plugin_base_t &plugin) {
        plugin.with_casted<r::plugin::link_client_plugin_t>([&](auto &p) {
            p.link(addr_s, false, [&](auto &ec) mutable {
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
    act_c->configurer = [&](auto &, r::plugin_base_t &plugin) {
        plugin.with_casted<r::plugin::link_client_plugin_t>([&](auto &p) {
            p.link(some_addr, false, [&](auto &ec) mutable {
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
    auto &addr_s = act_s->get_address();

    act_c->configurer = [&](auto &, r::plugin_base_t &plugin) {
        plugin.with_casted<r::plugin::link_client_plugin_t>([&](auto &p) { p.link(addr_s, false, [&](auto &) {}); });
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
            sup2->do_process();
            sup1->do_process();
            sup2->do_process();
            sup1->do_process();
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

TEST_CASE("auto-unlink on shutdown", "[actor]") {
    rt::system_context_test_t ctx1;
    rt::system_context_test_t ctx2;

    const char l1[] = "abc";
    const char l2[] = "def";

    auto sup1 = ctx1.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).locality(l1).finish();
    auto sup2 = ctx2.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).locality(l2).finish();

    auto act_c = sup1->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto act_s = sup2->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto &addr_s = act_s->get_address();

    act_c->configurer = [&](auto &, r::plugin_base_t &plugin) {
        plugin.with_casted<r::plugin::link_client_plugin_t>([&](auto &p) { p.link(addr_s, false, [&](auto &) {}); });
    };

    sup1->do_process();
    REQUIRE(act_c->get_state() == r::state_t::INITIALIZING);
    act_c->do_shutdown();

    sup1->do_process();
    REQUIRE(act_c->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup1->get_state() == r::state_t::SHUTTED_DOWN);

    sup2->do_process();
    REQUIRE(sup2->get_state() == r::state_t::OPERATIONAL);

    sup2->do_shutdown();
    sup2->do_process();
    REQUIRE(sup2->get_state() == r::state_t::SHUTTED_DOWN);
}

TEST_CASE("link to operational only", "[actor]") {
    rt::system_context_test_t ctx1;
    rt::system_context_test_t ctx2;
    rt::system_context_test_t ctx3;

    const char l1[] = "abc";
    const char l2[] = "def";
    const char l3[] = "ghi";

    auto sup1 = ctx1.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).locality(l1).finish();
    auto sup2 = ctx2.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).locality(l2).finish();
    auto sup3 = ctx3.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).locality(l3).finish();

    auto act_c = sup1->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto act_s1 = sup2->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto act_s2 = sup3->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();

    auto &addr_s1 = act_s1->get_address();
    auto &addr_s2 = act_s2->get_address();

    act_c->configurer = [&](auto &, r::plugin_base_t &plugin) {
        plugin.with_casted<r::plugin::link_client_plugin_t>([&](auto &p) { p.link(addr_s1, true, [&](auto &) {}); });
    };
    act_s1->configurer = [&](auto &, r::plugin_base_t &plugin) {
        plugin.with_casted<r::plugin::link_client_plugin_t>([&](auto &p) { p.link(addr_s2, true, [&](auto &) {}); });
    };

    auto process_12 = [&]() {
        while (!sup1->get_leader_queue().empty() || !sup2->get_leader_queue().empty()) {
            sup1->do_process();
            sup2->do_process();
        }
    };
    auto process_123 = [&]() {
        while (!sup1->get_leader_queue().empty() || !sup2->get_leader_queue().empty() ||
               !sup3->get_leader_queue().empty()) {
            sup1->do_process();
            sup2->do_process();
            sup3->do_process();
        }
    };

    process_12();
    CHECK(act_c->get_state() == r::state_t::INITIALIZING);
    CHECK(act_s1->get_state() == r::state_t::INITIALIZING);

    process_123();
    CHECK(act_c->get_state() == r::state_t::OPERATIONAL);
    CHECK(act_s1->get_state() == r::state_t::OPERATIONAL);
    CHECK(act_s2->get_state() == r::state_t::OPERATIONAL);

    sup1->do_shutdown();
    sup2->do_shutdown();
    sup3->do_shutdown();
    process_123();

    CHECK(act_c->get_state() == r::state_t::SHUTTED_DOWN);
    CHECK(act_s1->get_state() == r::state_t::SHUTTED_DOWN);
    CHECK(act_s2->get_state() == r::state_t::SHUTTED_DOWN);
}

TEST_CASE("unlink notify / response race", "[actor]") {
    rt::system_context_test_t system_context;

    const char l1[] = "abc";
    const char l2[] = "def";

    auto sup1 =
        system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).locality(l1).finish();
    auto sup2 = sup1->create_actor<rt::supervisor_test_t>().timeout(rt::default_timeout).locality(l2).finish();

    auto act_s = sup1->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto act_c = sup2->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto &addr_s = act_s->get_address();

    act_c->configurer = [&](auto &, r::plugin_base_t &plugin) {
        plugin.with_casted<r::plugin::link_client_plugin_t>([&](auto &p) { p.link(addr_s, true, [&](auto &) {}); });
    };
    while (!sup1->get_leader_queue().empty() || !sup2->get_leader_queue().empty()) {
        sup1->do_process();
        sup2->do_process();
    }
    REQUIRE(sup1->get_state() == r::state_t::OPERATIONAL);

    act_s->do_shutdown();
    act_c->do_shutdown();
    sup1->do_process();

    // extract unlink request to let it produce unlink notify
    auto unlink_request = sup2->get_leader_queue().back();
    REQUIRE(unlink_request->type_index == r::message::unlink_request_t::message_type);
    sup2->get_leader_queue().pop_back();
    sup2->do_process();

    sup1->do_shutdown();
    while (!sup1->get_leader_queue().empty() || !sup2->get_leader_queue().empty()) {
        sup1->do_process();
        sup2->do_process();
    }
    CHECK(sup1->active_timers.size() == 0);
    CHECK(sup1->get_state() == r::state_t::SHUTTED_DOWN);
}
