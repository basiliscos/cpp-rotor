//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "rotor/ev.hpp"
#include <ev.h>

namespace r = rotor;
namespace re = rotor::ev;
//namespace rt = rotor::test;

struct ping_t{};
struct pong_t{};

static std::uint32_t destroyed = 0;

struct pinger_t : public r::actor_base_t {
    std::uint32_t ping_sent;
    std::uint32_t pong_received;
    rotor::address_ptr_t ponger_addr;

  pinger_t(rotor::supervisor_t &sup): r::actor_base_t{sup} {
      ping_sent = pong_received = 0;
  }
  ~pinger_t() { destroyed += 1; }

  void set_ponger_addr(const rotor::address_ptr_t &addr) { ponger_addr = addr; }

  void on_initialize(r::message_t<r::payload::initialize_actor_t>&msg) noexcept override {
    r::actor_base_t::on_initialize(msg);
    subscribe(&pinger_t::on_pong);
  }

  void on_start(rotor::message_t<rotor::payload::start_actor_t> &msg) noexcept override {
      r::actor_base_t::on_start(msg);
      send<ping_t>(ponger_addr);
      ++ping_sent;
  }

  void on_pong(rotor::message_t<pong_t> &) noexcept {
      ++pong_received;
      supervisor.shutdown();
  }
};

struct ponger_t : public r::actor_base_t {
    std::uint32_t pong_sent;
    std::uint32_t ping_received;
    rotor::address_ptr_t pinger_addr;

  ponger_t(rotor::supervisor_t &sup) : rotor::actor_base_t{sup} {
      pong_sent = ping_received = 0;
  }
  ~ponger_t() { destroyed += 2; }

  void set_pinger_addr(const rotor::address_ptr_t &addr) { pinger_addr = addr; }

  void on_initialize(rotor::message_t<rotor::payload::initialize_actor_t>
                         &msg) noexcept override {
    rotor::actor_base_t::on_initialize(msg);
    subscribe(&ponger_t::on_ping);
  }

  void on_ping(rotor::message_t<ping_t> &) noexcept {
    ++ping_received;
    send<pong_t>(pinger_addr);
    ++pong_sent;
  }
};

struct supervisor_ev_test_t : public re::supervisor_ev_t {
    using re::supervisor_ev_t::supervisor_ev_t;

    ~supervisor_ev_test_t() { destroyed += 4; }

    r::state_t& get_state() noexcept { return state; }
    queue_t& get_queue() noexcept { return outbound; }
    queue_t& get_inbound_queue() noexcept { return inbound; }
    subscription_points_t& get_points() noexcept { return points; }
    subscription_map_t& get_subscription() noexcept { return subscription_map; }
};


TEST_CASE("ping/pong", "[supervisor][ev]") {
    auto* loop = ev_loop_new(0);
    auto system_context = re::system_context_ev_t::ptr_t{new re::system_context_ev_t()};
    auto conf = re::supervisor_config_t{loop, true, 1.0};
    auto sup = system_context->create_supervisor<supervisor_ev_test_t>(conf);

    sup->start();

    auto pinger = sup->create_actor<pinger_t>();
    auto ponger = sup->create_actor<ponger_t>();
    pinger->set_ponger_addr(ponger->get_address());
    ponger->set_pinger_addr(pinger->get_address());

    sup->start();
    ev_run(loop);

    REQUIRE(pinger->ping_sent == 1);
    REQUIRE(pinger->pong_received == 1);
    REQUIRE(ponger->pong_sent == 1);
    REQUIRE(ponger->ping_received == 1);

    pinger.reset();
    ponger.reset();

    sup.reset();
    system_context.reset();
    //ev_loop_destroy(loop);

    REQUIRE(destroyed == 1 + 2 + 4);
}
