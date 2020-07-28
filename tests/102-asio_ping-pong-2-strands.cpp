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
#include "access.h"

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
    }

    void on_pong(r::message_t<pong_t> &) noexcept {
        ++pong_received;
        supervisor->shutdown();
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

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&ponger_t::on_ping); });
    }

    void on_ping(rotor::message_t<ping_t> &) noexcept {
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
    auto strand1 = std::make_shared<asio::io_context::strand>(io_context);
    auto strand2 = std::make_shared<asio::io_context::strand>(io_context);
    auto timeout = r::pt::milliseconds{10};

    auto sup1 =
        system_context->create_supervisor<rt::supervisor_asio_test_t>().strand(strand1).timeout(timeout).finish();
    auto sup2 = sup1->create_actor<rt::supervisor_asio_test_t>().strand(strand2).timeout(timeout).finish();

    auto pinger = sup1->create_actor<pinger_t>().timeout(timeout).finish();
    auto ponger = sup2->create_actor<ponger_t>().timeout(timeout).finish();

    pinger->set_ponger_addr(static_cast<r::actor_base_t *>(ponger.get())->get_address());
    ponger->set_pinger_addr(static_cast<r::actor_base_t *>(pinger.get())->get_address());

    sup1->start();
    io_context.run();

    REQUIRE(pinger->ping_sent == 1);
    REQUIRE(pinger->pong_received == 1);
    REQUIRE(ponger->ping_received == 1);
    REQUIRE(ponger->pong_sent == 1);

    REQUIRE(static_cast<r::actor_base_t *>(sup1.get())->access<rt::to::state>() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup1->get_leader_queue().size() == 0);
    CHECK(rt::empty(sup1->get_subscription()));

    REQUIRE(static_cast<r::actor_base_t *>(sup2.get())->access<rt::to::state>() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup2->get_leader_queue().size() == 0);
    CHECK(rt::empty(sup2->get_subscription()));

    REQUIRE(pinger->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(ponger->get_state() == r::state_t::SHUTTED_DOWN);
}
