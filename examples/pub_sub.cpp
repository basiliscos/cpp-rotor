#include "rotor.hpp"
#include <iostream>

namespace r = rotor;

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
  using r::actor_base_t::actor_base_t;

  void set_pub_addr(const r::address_ptr_t &addr) { pub_addr = addr; }

  void on_initialize(r::message_t<r::payload::initialize_actor_t>
                         &msg) noexcept override {
    r::actor_base_t::on_initialize(msg);
    subscribe(&sub_t::on_payload, pub_addr);
  }

  void on_payload(r::message_t<payload_t> &) noexcept {
       std::cout << "received on " << static_cast<void*>(this) << "\n";
  }

  r::address_ptr_t pub_addr;
};

struct dummy_supervisor : public rotor::supervisor_t {
    void start_shutdown_timer() noexcept override {}
    void cancel_shutdown_timer() noexcept override {}
    void start() noexcept override {}
    void shutdown() noexcept override {}
    void enqueue(rotor::message_ptr_t) noexcept override {}
};

int main() {
    rotor::system_context_t ctx{};
    auto sup = ctx.create_supervisor<dummy_supervisor>();

    auto pub_addr = sup->create_address(); // (1)
    auto pub = sup->create_actor<pub_t>();
    auto sub1 = sup->create_actor<sub_t>();
    auto sub2 = sup->create_actor<sub_t>();

    pub->set_pub_addr(pub_addr);
    sub1->set_pub_addr(pub_addr);
    sub2->set_pub_addr(pub_addr);

    sup->do_process();
    return 0;
}
