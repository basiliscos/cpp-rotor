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

struct payload_t{};

struct pub_t : public r::actor_base_t {

  using r::actor_base_t::actor_base_t;

  void set_pub_addr(const r::address_ptr_t &addr) { pub_addr = addr; }

  void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
      r::actor_base_t::on_start(msg);
      send<payload_t>(pub_addr);
  }

  r::address_ptr_t pub_addr;
};

struct sub_t : public r::actor_base_t {
  std::uint16_t received = 0;
  using r::actor_base_t::actor_base_t;

  void set_pub_addr(const r::address_ptr_t &addr) { pub_addr = addr; }

  void on_initialize(r::message_t<r::payload::initialize_actor_t>
                         &msg) noexcept override {
    r::actor_base_t::on_initialize(msg);
    subscribe(&sub_t::on_payload, pub_addr);
  }

  void on_payload(r::message_t<payload_t> &) noexcept {
      ++received;
  }

  r::address_ptr_t pub_addr;
};



TEST_CASE("ping-pong", "[supervisor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>(nullptr, nullptr);
    auto pub_addr = sup->create_address();
    auto pub = sup->create_actor<pub_t>();
    auto sub1 = sup->create_actor<sub_t>();
    auto sub2 = sup->create_actor<sub_t>();

    pub->set_pub_addr(pub_addr);
    sub1->set_pub_addr(pub_addr);
    sub2->set_pub_addr(pub_addr);

    sup->do_start();
    sup->do_process();

    REQUIRE(sub1->received == 1);
    REQUIRE(sub2->received == 1);

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().size() == 0);
}

