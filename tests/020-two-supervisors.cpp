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

TEST_CASE("two supervisors", "[supervisor]") {
    r::system_context_t system_context;

    auto sup1 = system_context.create_supervisor<rt::supervisor_test_t>();
    auto sup2 = sup1->create_actor<rt::supervisor_test_t>();

    REQUIRE(&sup2->get_supervisor() == sup2.get());
    REQUIRE(sup2->get_parent_supervisor() == sup1.get());

    sup1->do_start();
    sup1->do_process();
    REQUIRE(sup1->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(sup2->get_state() == r::state_t::INITIALIZED);

    sup2->do_process();
    REQUIRE(sup2->get_state() == r::state_t::OPERATIONAL);

    sup1->do_shutdown();
    sup1->do_process();

    REQUIRE(sup1->get_state() == r::state_t::SHUTTING_DOWN);
    REQUIRE(sup2->get_state() == r::state_t::OPERATIONAL);

    sup2->do_process();
    REQUIRE(sup1->get_state() == r::state_t::SHUTTING_DOWN);
    REQUIRE(sup2->get_state() == r::state_t::SHUTTED_DOWN);

    sup1->do_process();
    REQUIRE(sup1->get_state() == r::state_t::SHUTTED_DOWN);

    REQUIRE(sup1->get_queue().size() == 0);
    REQUIRE(sup1->get_points().size() == 0);
    REQUIRE(sup1->get_subscription().size() == 0);

    REQUIRE(sup2->get_queue().size() == 0);
    REQUIRE(sup2->get_points().size() == 0);
    REQUIRE(sup2->get_subscription().size() == 0);
}
