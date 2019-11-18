//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor.hpp"
#include <iostream>

namespace r = rotor;

struct payload_t {};
using sample_message_t = r::message_t<payload_t>;

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

    void init_start() noexcept override {
        subscribe(&sub_t::on_payload, pub_addr);
        rotor::actor_base_t::init_start();
    }

    void on_payload(sample_message_t &) noexcept { std::cout << "received on " << static_cast<void *>(this) << "\n"; }

    r::address_ptr_t pub_addr;
};

struct dummy_supervisor : public rotor::supervisor_t {
    using rotor::supervisor_t::supervisor_t;
    void start_timer(const rotor::pt::time_duration &, timer_id_t) noexcept override {}
    void cancel_timer(timer_id_t) noexcept override {}
    void start() noexcept override {}
    void shutdown() noexcept override {}
    void enqueue(rotor::message_ptr_t) noexcept override {}
};

int main() {
    rotor::system_context_t ctx{};
    auto timeout = boost::posix_time::milliseconds{500}; /* does not matter */
    auto sup = ctx.create_supervisor<dummy_supervisor>().timeout(timeout).finish();

    auto pub_addr = sup->create_address(); // (1)
    auto pub = sup->create_actor<pub_t>().timeout(timeout).finish();
    auto sub1 = sup->create_actor<sub_t>().timeout(timeout).finish();
    auto sub2 = sup->create_actor<sub_t>().timeout(timeout).finish();

    pub->set_pub_addr(pub_addr);
    sub1->set_pub_addr(pub_addr);
    sub2->set_pub_addr(pub_addr);

    sup->do_process();

    sup->do_shutdown();
    sup->do_process();

    return 0;
}
