//
// Copyright (c) 2019-2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor.hpp"
#include "dummy_supervisor.h"
#include <iostream>

namespace payload {

struct ping_t {};
struct pong_t {};

} // namespace payload

namespace message {
using ping_t = rotor::message_t<payload::ping_t>;
using pong_t = rotor::message_t<payload::pong_t>;
} // namespace message

struct lambda_pinger_t : public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;

    void set_ponger_addr(const rotor::address_ptr_t &addr) { ponger_addr = addr; }

    void configure(rotor::plugin::plugin_base_t &plugin) noexcept override {
        rotor::actor_base_t::configure(plugin);
        plugin.with_casted<rotor::plugin::starter_plugin_t>([&](auto &p) {
            auto handler = rotor::lambda<message::pong_t>([&](auto &) noexcept {
                std::cout << "pong\n";
                do_shutdown();
            });
            p.subscribe_actor(std::move(handler));
        });
    }

    void on_start() noexcept override {
        rotor::actor_base_t::on_start();
        send<payload::ping_t>(ponger_addr);
    }

    rotor::address_ptr_t ponger_addr;
};

struct ponger_t : public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;
    void set_pinger_addr(const rotor::address_ptr_t &addr) { pinger_addr = addr; }

    void configure(rotor::plugin::plugin_base_t &plugin) noexcept override {
        rotor::actor_base_t::configure(plugin);
        plugin.with_casted<rotor::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&ponger_t::on_ping); });
    }

    void on_ping(message::ping_t &) noexcept {
        std::cout << "ping\n";
        send<payload::pong_t>(pinger_addr);
    }

  private:
    rotor::address_ptr_t pinger_addr;
};

int main() {
    rotor::system_context_t ctx{};
    auto timeout = boost::posix_time::milliseconds{500}; /* does not matter */
    auto sup = ctx.create_supervisor<dummy_supervisor_t>().timeout(timeout).finish();

    auto pinger = sup->create_actor<lambda_pinger_t>().timeout(timeout).autoshutdown_supervisor().finish();
    auto ponger = sup->create_actor<ponger_t>().timeout(timeout).finish();
    pinger->set_ponger_addr(ponger->get_address());
    ponger->set_pinger_addr(pinger->get_address());

    sup->do_process();
    return 0;
}
