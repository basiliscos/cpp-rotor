//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor.hpp"
#include "dummy_supervisor.h"
#include <iostream>

namespace r = rotor;

struct payload_t {};
using sample_message_t = r::message_t<payload_t>;

struct pub_t : public r::actor_base_t {

    using r::actor_base_t::actor_base_t;

    void set_pub_addr(const r::address_ptr_t &addr) { pub_addr = addr; }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        send<payload_t>(pub_addr);
    }

    r::address_ptr_t pub_addr;
};

struct sub_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;

    void set_pub_addr(const r::address_ptr_t &addr) { pub_addr = addr; }

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        rotor::actor_base_t::configure(plugin);
        plugin.with_casted<r::plugin::starter_plugin_t>(
            [&](auto &p) { p.subscribe_actor(&sub_t::on_payload, pub_addr); });
    }

    void on_payload(sample_message_t &) noexcept { std::cout << "received on " << static_cast<void *>(this) << "\n"; }

    r::address_ptr_t pub_addr;
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
