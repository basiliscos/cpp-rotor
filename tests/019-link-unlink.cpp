//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "actor_test.h"
#include "supervisor_test.h"

namespace r = rotor;
namespace rt = r::test;

TEST_CASE("client/server, common workflow", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act_s = sup->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).unlink_timeout(rt::default_timeout).finish();
    auto act_c = sup->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();

    auto server_addr = act_s->get_address();
    auto client_addr = act_c->get_address();

    act_c->link_request(server_addr, rt::default_timeout);
    REQUIRE(act_c->get_behaviour().get_linking_actors().size() == 1);
    REQUIRE(act_c->get_linked_clients().size() == 0);
    REQUIRE(act_c->get_linked_servers().size() == 0);
    REQUIRE(act_s->get_linked_clients().size() == 0);
    REQUIRE(act_s->get_linked_servers().size() == 0);

    sup->do_process();
    REQUIRE(act_s->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(act_c->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(act_c->get_behaviour().get_linking_actors().size() == 0);
    REQUIRE(act_c->get_linked_clients().size() == 0);
    REQUIRE(act_c->get_linked_servers().size() == 1);
    REQUIRE(act_c->get_linked_servers().count(server_addr) == 1);
    REQUIRE(act_s->get_linked_clients().size() == 1);
    REQUIRE(act_s->get_linked_clients().count(r::details::linkage_t{server_addr, client_addr}) == 1);
    REQUIRE(act_s->get_linked_servers().size() == 0);

    SECTION("direct client-initiated unlink") {
        act_c->unlink_notify(server_addr);
        sup->do_process();
        REQUIRE(act_c->get_linked_clients().size() == 0);
        REQUIRE(act_c->get_linked_servers().size() == 0);
        REQUIRE(act_s->get_linked_clients().size() == 0);
        REQUIRE(act_s->get_linked_servers().size() == 0);
    }

    SECTION("indirect client-initiated unlink via client-shutdown") {
        act_c->do_shutdown();
        sup->do_process();
        REQUIRE(act_c->get_linked_clients().size() == 0);
        REQUIRE(act_c->get_linked_servers().size() == 0);
        REQUIRE(act_s->get_linked_clients().size() == 0);
        REQUIRE(act_s->get_linked_servers().size() == 0);
    }

    SECTION("indirect client-initiated unlink via server-shutdown") {
        act_s->do_shutdown();
        sup->do_process();
        REQUIRE(act_c->get_linked_clients().size() == 0);
        REQUIRE(act_c->get_linked_servers().size() == 0);
        REQUIRE(act_s->get_linked_clients().size() == 0);
        REQUIRE(act_s->get_linked_servers().size() == 0);
    }

    sup->do_shutdown();
    sup->do_process();
}

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
