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

struct sample_actor_t : public rt::actor_test_t {

    using rt::actor_test_t::actor_test_t;

    virtual void init_start() noexcept override {}
    virtual void do_init_start() noexcept {
        r::actor_base_t::init_start();
    }

};

TEST_CASE("supervisor is not initialized, while it child did not confirmed initialization", "[supervisor]") {
    r::system_context_t system_context;
    const void *locality = &system_context;

    auto timeout = r::pt::milliseconds{1};
    rt::supervisor_config_test_t config(timeout, locality);
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>(nullptr, config);
    auto act = sup->create_actor<sample_actor_t>(timeout);

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act->get_state() == r::state_t::INITIALIZING);
/*
    REQUIRE(sup_A2->get_children().size() == 1);
    REQUIRE(sup_B2->get_children().size() == 0);
    REQUIRE(ping_sent == 1);
    REQUIRE(ping_received == 1);

    sup_root->do_shutdown();
    sup_root->do_process();
*/

/*
    auto sup_B1 = sup_root->create_actor<rt::supervisor_test_t>(timeout, config);
    auto sup_B2 = sup_B1->create_actor<rt::supervisor_test_t>(timeout, config);

    auto pinger = sup_A2->create_actor<pinger_t>(timeout);
    auto ponger = sup_B2->create_actor<ponger_t>(timeout);

    pinger->set_ponger_addr(ponger->get_address());
    sup_A2->do_process();

    REQUIRE(sup_A2->get_children().size() == 1);
    REQUIRE(sup_B2->get_children().size() == 0);
    REQUIRE(ping_sent == 1);
    REQUIRE(ping_received == 1);

    sup_root->do_shutdown();
    sup_root->do_process();

    REQUIRE(sup_A2->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup_B2->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup_A1->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup_B1->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup_root->get_state() == r::state_t::SHUTTED_DOWN);
*/
}
