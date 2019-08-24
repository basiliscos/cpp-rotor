//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "supervisor_test.h"
#include <iostream>

namespace r = rotor;
namespace rt = r::test;

struct ping_t{};
struct pong_t{};


struct pinger_t : public r::actor_base_t {
    std::uint32_t ping_sent;
    std::uint32_t pong_received;

  pinger_t(r::supervisor_t &sup) : r::actor_base_t{sup}{
      ping_sent = pong_received = 0;
  }

  void set_ponger_addr(const r::address_ptr_t &addr) { ponger_addr = addr; }

  void on_initialize(r::message_t<r::payload::initialize_actor_t>
                         &msg) noexcept override {
    r::actor_base_t::on_initialize(msg);
    subscribe(&pinger_t::on_pong);
  }

  void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
      r::actor_base_t::on_start(msg);
  }

  void on_pong(r::message_t<pong_t> &) noexcept {
      ++pong_received;
  }

  void do_send_ping() {
      ++ping_sent;
      send<ping_t>(ponger_addr);
  }

  r::address_ptr_t ponger_addr;
};

struct pinger_autostart_t : public pinger_t {
    using pinger_t::pinger_t;

    void on_initialize(r::message_t<r::payload::initialize_actor_t>
                           &msg) noexcept override {
      pinger_t::on_initialize(msg);
      subscribe(&pinger_autostart_t::on_ponger_start, ponger_addr);
    }

    void on_ponger_start(r::message_t<r::payload::start_actor_t>&) noexcept {
        do_send_ping();
    }

};

struct ponger_t : public r::actor_base_t {
    std::uint32_t ping_received;
    std::uint32_t pong_sent;

  ponger_t(r::supervisor_t &sup) : r::actor_base_t{sup} {
      ping_received = pong_sent = 0;
  }

  void set_pinger_addr(const r::address_ptr_t &addr) { pinger_addr = addr; }

  void on_initialize(r::message_t<r::payload::initialize_actor_t>
                         &msg) noexcept override {
    r::actor_base_t::on_initialize(msg);
    subscribe(&ponger_t::on_ping);
  }

  void on_start(r::message_t<r::payload::start_actor_t> &) noexcept override {
      std::cout << "on_start\n";
  }

  void on_ping(r::message_t<ping_t> &) noexcept {
    ++ping_received;
    send<pong_t>(pinger_addr);
    ++pong_sent;
  }

private:
  r::address_ptr_t pinger_addr;
};

TEST_CASE("pinger & ponger on different supervisors, manually controlled", "[supervisor]") {
    r::system_context_t system_context;

    const char locality1[] = "l1";
    const char locality2[] = "l2";
    auto sup1 = system_context.create_supervisor<rt::supervisor_test_t>(nullptr, locality1);
    auto sup2 = sup1->create_actor<rt::supervisor_test_t>(locality2);

    auto pinger = sup1->create_actor<pinger_t>();
    auto ponger = sup2->create_actor<ponger_t>();

    pinger->set_ponger_addr(ponger->get_address());
    ponger->set_pinger_addr(pinger->get_address());

    sup1->do_process();
    sup2->do_process();
    REQUIRE(pinger->ping_sent == 0);
    REQUIRE(pinger->pong_received == 0);
    REQUIRE(ponger->ping_received == 0);
    REQUIRE(ponger->pong_sent == 0);

    pinger->do_send_ping();
    sup1->do_process();
    REQUIRE(pinger->ping_sent == 1);
    REQUIRE(pinger->pong_received == 0);
    REQUIRE(ponger->ping_received == 0);
    REQUIRE(ponger->pong_sent == 0);

    sup2->do_process();
    REQUIRE(pinger->ping_sent == 1);
    REQUIRE(pinger->pong_received == 0);
    REQUIRE(ponger->ping_received == 1);
    REQUIRE(ponger->pong_sent == 1);

    sup1->do_process();
    REQUIRE(pinger->ping_sent == 1);
    REQUIRE(pinger->pong_received == 1);
    REQUIRE(ponger->ping_received == 1);
    REQUIRE(ponger->pong_sent == 1);

    sup1->do_shutdown();
    while (!sup1->get_queue().empty() || !sup2->get_queue().empty()) {
        sup1->do_process();
        sup2->do_process();
    }
    REQUIRE(sup2->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup2->get_queue().size() == 0);
    REQUIRE(sup2->get_points().size() == 0);
    REQUIRE(sup2->get_subscription().size() == 0);

    REQUIRE(sup1->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup1->get_queue().size() == 0);
    REQUIRE(sup1->get_points().size() == 0);
    REQUIRE(sup1->get_subscription().size() == 0);
}

TEST_CASE("pinger & ponger on different supervisors, self controlled", "[supervisor]") {
    r::system_context_t system_context;

    const char locality1[] = "l1";
    const char locality2[] = "l2";
    auto sup1 = system_context.create_supervisor<rt::supervisor_test_t>(nullptr, locality1);
    auto sup2 = sup1->create_actor<rt::supervisor_test_t>(locality2);

    auto pinger = sup1->create_actor<pinger_autostart_t>();
    auto ponger = sup2->create_actor<ponger_t>();

    pinger->set_ponger_addr(ponger->get_address());
    ponger->set_pinger_addr(pinger->get_address());

    sup1->do_process();
    sup2->do_process();
    REQUIRE(pinger->ping_sent == 0);
    REQUIRE(pinger->pong_received == 0);
    REQUIRE(ponger->ping_received == 0);
    REQUIRE(ponger->pong_sent == 0);

    sup1->do_process();
    REQUIRE(pinger->ping_sent == 1);
    REQUIRE(pinger->pong_received == 0);
    REQUIRE(ponger->ping_received == 0);
    REQUIRE(ponger->pong_sent == 0);

    sup2->do_process();
    REQUIRE(pinger->ping_sent == 1);
    REQUIRE(pinger->pong_received == 0);
    REQUIRE(ponger->ping_received == 1);
    REQUIRE(ponger->pong_sent == 1);

    sup1->do_process();
    REQUIRE(pinger->ping_sent == 1);
    REQUIRE(pinger->pong_received == 1);
    REQUIRE(ponger->ping_received == 1);
    REQUIRE(ponger->pong_sent == 1);

    sup1->do_shutdown();
    while (!sup1->get_queue().empty() || !sup2->get_queue().empty()) {
        sup1->do_process();
        sup2->do_process();
    }
    REQUIRE(sup2->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup2->get_queue().size() == 0);
    REQUIRE(sup2->get_points().size() == 0);
    REQUIRE(sup2->get_subscription().size() == 0);

    REQUIRE(sup1->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup1->get_queue().size() == 0);
    REQUIRE(sup1->get_points().size() == 0);
    REQUIRE(sup1->get_subscription().size() == 0);
}
