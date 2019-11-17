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

static std::uint32_t ping_received = 0;
static std::uint32_t ping_sent = 0;

struct ping_t {};

struct pinger_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;

    void set_ponger_addr(const r::address_ptr_t &addr) { ponger_addr = addr; }

    void init_start() noexcept override {
        subscribe(&pinger_t::on_state);
        r::actor_base_t::init_start();
    }

    void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        r::actor_base_t::on_start(msg);
        request_status();
    }

    void request_status() noexcept {
        auto reply_addr = get_address();
        request<r::payload::state_request_t>(ponger_addr->supervisor.get_address(), ponger_addr)
            .send(r::pt::seconds{1});
        ++attempts;
    }

    void on_state(r::message::state_response_t &msg) noexcept {
        auto &state = msg.payload.res.state;
        if (state == r::state_t::OPERATIONAL) {
            send<ping_t>(ponger_addr);
            ponger_addr.reset();
            ping_sent++;
        } else if (attempts > 10) {
            do_shutdown();
        } else {
            request_status();
        }
    }

    std::uint32_t attempts = 0;
    r::address_ptr_t ponger_addr;
};

struct ponger_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;

    void init_start() noexcept override {
        subscribe(&ponger_t::on_ping);
        r::actor_base_t::init_start();
    }

    void on_ping(r::message_t<ping_t> &) noexcept {
        ping_received++;
        do_shutdown();
    }
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
}
