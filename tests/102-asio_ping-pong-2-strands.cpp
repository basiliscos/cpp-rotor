//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "rotor/asio.hpp"
#include "supervisor_asio_test.h"
#include "actor_test.h"

#include <iostream>

namespace r = rotor;
namespace ra = rotor::asio;
namespace rt = r::test;
namespace asio = boost::asio;
namespace pt = boost::posix_time;

struct ping_t {};
struct pong_t {};

struct pinger_t : public rt::actor_test_t {

    std::uint32_t ping_sent;
    std::uint32_t pong_received;
    std::uint32_t request_attempts;

    pinger_t(r::supervisor_t &sup) : rt::actor_test_t{sup} { request_attempts = ping_sent = pong_received = 0; }

    void set_ponger_addr(const r::address_ptr_t &addr) { ponger_addr = addr; }

    void init_start() noexcept override {
        subscribe(&pinger_t::on_pong);
        subscribe(&pinger_t::on_ponger_start, ponger_addr);
        subscribe(&pinger_t::on_state);
        request_ponger_status();
    }

    void inline request_ponger_status() noexcept {
        ++request_attempts;
        request<r::payload::state_request_t>(ponger_addr->supervisor.get_address(), ponger_addr)
            .send(r::pt::seconds{1});
    }

    void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        r::actor_base_t::on_start(msg);
        unsubscribe(&pinger_t::on_ponger_start, ponger_addr);
        unsubscribe(&pinger_t::on_state);
        do_send_ping();
    }

    void on_pong(r::message_t<pong_t> &) noexcept {
        ++pong_received;
        supervisor.shutdown();
    }

    void on_ponger_start(r::message_t<r::payload::start_actor_t> &) noexcept {
        if (state == r::state_t::INITIALIZING) {
            rt::actor_test_t::init_start();
        }
        // we already get the right
    }

    void on_state(r::message::state_response_t &msg) noexcept {
        auto &target_state = msg.payload.res.state;
        if (state == r::state_t::INITIALIZED) {
            return; // we are already  on_ponger_start
        }
        if (target_state == r::state_t::OPERATIONAL) {
            rt::actor_test_t::init_start();
        } else {
            if (request_attempts > 3) {
                do_shutdown();
            } else {
                request_ponger_status();
            }
        }
    }

    void do_send_ping() {
        ++ping_sent;
        send<ping_t>(ponger_addr);
    }

    r::address_ptr_t ponger_addr;
};

struct ponger_t : public rt::actor_test_t {
    std::uint32_t ping_received;
    std::uint32_t pong_sent;

    ponger_t(r::supervisor_t &sup) : rt::actor_test_t{sup} { ping_received = pong_sent = 0; }

    void set_pinger_addr(const r::address_ptr_t &addr) { pinger_addr = addr; }

    void init_start() noexcept override {
        subscribe(&ponger_t::on_ping);
        r::actor_base_t::init_start();
    }

    void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        std::cout << "on_start\n";
        r::actor_base_t::on_start(msg);
    }

    void on_ping(r::message_t<ping_t> &) noexcept {
        ++ping_received;
        send<pong_t>(pinger_addr);
        ++pong_sent;
    }

  private:
    r::address_ptr_t pinger_addr;
};

TEST_CASE("ping/pong 2 sups", "[supervisor][asio]") {
    asio::io_context io_context{1};
    auto system_context = ra::system_context_asio_t::ptr_t{new ra::system_context_asio_t(io_context)};
    auto stand = std::make_shared<asio::io_context::strand>(io_context);
    auto timeout = r::pt::milliseconds{10};
    ra::supervisor_config_asio_t conf{timeout, std::move(stand)};

    auto sup1 = system_context->create_supervisor<rt::supervisor_asio_test_t>(conf);
    auto sup2 = sup1->create_actor<rt::supervisor_asio_test_t>(timeout, conf);

    auto pinger = sup1->create_actor<pinger_t>(timeout);
    auto ponger = sup2->create_actor<ponger_t>(timeout);

    pinger->set_ponger_addr(ponger->get_address());
    ponger->set_pinger_addr(pinger->get_address());

    sup1->start();
    io_context.run();

    REQUIRE(pinger->ping_sent == 1);
    REQUIRE(pinger->pong_received == 1);
    REQUIRE(ponger->ping_received == 1);
    REQUIRE(ponger->pong_sent == 1);

    REQUIRE(sup1->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup1->get_leader_queue().size() == 0);
    REQUIRE(sup1->get_points().size() == 0);
    REQUIRE(sup1->get_subscription().size() == 0);

    REQUIRE(sup2->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup2->get_leader_queue().size() == 0);
    REQUIRE(sup2->get_points().size() == 0);
    REQUIRE(sup2->get_subscription().size() == 0);

    REQUIRE(pinger->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(ponger->get_state() == r::state_t::SHUTTED_DOWN);

    REQUIRE(pinger->get_points().size() == 0);
    REQUIRE(ponger->get_points().size() == 0);
}
