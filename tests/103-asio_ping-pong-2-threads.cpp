//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
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

    std::uint32_t ping_sent = 0;
    std::uint32_t pong_received = 0;

    using rt::actor_test_t::actor_test_t;

    void set_ponger_addr(const r::address_ptr_t &addr) { ponger_addr = addr; }

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<r::plugin::starter_plugin_t>([&](auto &p) { p.subscribe_actor(&pinger_t::on_pong); });
        plugin.with_casted<r::plugin::link_client_plugin_t>(
            [&](auto &p) { p.link(ponger_addr, true, [&](auto &ec) mutable { REQUIRE(!ec); }); });
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        do_send_ping();
        std::cout << "pinger start\n";
    }

    void shutdown_finish() noexcept override {
        r::actor_base_t::shutdown_finish();
        supervisor->shutdown();
        ponger_addr->supervisor.shutdown();
        std::cout << "pinger shutdown finish\n";
    }

    void on_pong(r::message_t<pong_t> &) noexcept {
        ++pong_received;
        do_shutdown();
    }

    void do_send_ping() {
        ++ping_sent;
        send<ping_t>(ponger_addr);
    }

    r::address_ptr_t ponger_addr;
};

struct ponger_t : public rt::actor_test_t {
    std::uint32_t ping_received = 0;
    std::uint32_t pong_sent = 0;

    using rt::actor_test_t::actor_test_t;

    void set_pinger_addr(const r::address_ptr_t &addr) { pinger_addr = addr; }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        std::cout << "ponger start\n";
    }

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&ponger_t::on_ping); });
    }

    void on_ping(r::message_t<ping_t> &) noexcept {
        ++ping_received;
        send<pong_t>(pinger_addr);
        ++pong_sent;
    }

  private:
    r::address_ptr_t pinger_addr;
};

TEST_CASE("ping/pong on 2 threads", "[supervisor][asio]") {
    using sup_t = rt::supervisor_asio_test_t;
    asio::io_context io_ctx1;
    asio::io_context io_ctx2;

    auto timeout = r::pt::milliseconds{10};
    auto sys_ctx1 = ra::system_context_asio_t::ptr_t{new ra::system_context_asio_t(io_ctx1)};
    auto sys_ctx2 = ra::system_context_asio_t::ptr_t{new ra::system_context_asio_t(io_ctx2)};
    auto strand1 = std::make_shared<asio::io_context::strand>(io_ctx1);
    auto strand2 = std::make_shared<asio::io_context::strand>(io_ctx2);
    auto sup1 = sys_ctx1->create_supervisor<sup_t>().strand(strand1).timeout(timeout).guard_context(true).finish();
    auto sup2 = sys_ctx2->create_supervisor<sup_t>().strand(strand2).timeout(timeout).guard_context(true).finish();

    auto pinger = sup1->create_actor<pinger_t>().timeout(timeout).finish();
    auto ponger = sup2->create_actor<ponger_t>().timeout(timeout).finish();

    std::cout << "sup1: " << sup1.get() << ", sup2: " << sup2.get() << "\n";
    std::cout << "pinger: " << pinger.get() << ", ponger: " << ponger.get() << "\n";
    auto &pinger_addr = static_cast<r::actor_base_t *>(pinger.get())->get_address();
    auto &ponger_addr = static_cast<r::actor_base_t *>(ponger.get())->get_address();
    std::cout << "pinger addr: " << pinger_addr.get() << ", ponger addr: " << ponger_addr.get() << "\n";

    pinger->set_ponger_addr(static_cast<r::actor_base_t *>(ponger.get())->get_address());
    ponger->set_pinger_addr(static_cast<r::actor_base_t *>(pinger.get())->get_address());

    sup1->start();
    sup2->start();

    auto t1 = std::thread([&] { io_ctx1.run(); });
    auto t2 = std::thread([&] { io_ctx2.run(); });
    t1.join();
    t2.join();

    if (pinger->ping_sent == 1) {
        CHECK(pinger->ping_sent == 1);
        CHECK(pinger->pong_received == 1);
        CHECK(ponger->ping_received == 1);
        CHECK(ponger->pong_sent == 1);
    } else {
        // this might happen due to unavoidable race between threads, system scheduler etc.
        // can be artificially archived launching tool like `stress-ng --cpu 8`
        // and/or set small timeout
        // no special acetion is needed, as the pinger will trigger shutdown for the
        // both threads
    }

    CHECK(static_cast<r::actor_base_t *>(sup1.get())->access<rt::to::state>() == r::state_t::SHUT_DOWN);
    CHECK(sup1->get_leader_queue().size() == 0);
    CHECK(rt::empty(sup1->get_subscription()));

    CHECK(static_cast<r::actor_base_t *>(sup2.get())->access<rt::to::state>() == r::state_t::SHUT_DOWN);
    CHECK(sup2->get_leader_queue().size() == 0);
    CHECK(rt::empty(sup2->get_subscription()));

    REQUIRE(pinger->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(ponger->get_state() == r::state_t::SHUT_DOWN);
}
