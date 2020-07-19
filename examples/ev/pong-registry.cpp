//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <rotor/ev.hpp>
#include <rotor/registry.h>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <cstdlib>

/* NB: there is some race-condition, as the ponger might be register self in the registry
 * later then pinger asks for ponger address; then pinger receives discovery falilure
 * reply and shuts self down.
 *
 * The correct behavior will be introduced later in rotor with erlang-like actor restart
 * policy, i.e.: pinger should be downed, and it's supervisor should restart if (may be
 * a few times, may be with some delay in between).
 */

namespace payload {
struct pong_t {};
struct ping_t {
    using response_t = pong_t;
};
} // namespace payload

namespace message {
using ping_t = rotor::request_traits_t<payload::ping_t>::request::message_t;
using pong_t = rotor::request_traits_t<payload::ping_t>::response::message_t;
} // namespace message

static const char ponger_name[] = "service:ponger";
static const auto timeout = boost::posix_time::milliseconds{5};

struct pinger_t : public rotor::actor_base_t {

    using rotor::actor_base_t::actor_base_t;

    void configure(rotor::plugin_t &plugin) noexcept override {
        rotor::actor_base_t::configure(plugin);
        plugin.with_casted<rotor::internal::starter_plugin_t>([](auto &p) { p.subscribe_actor(&pinger_t::on_pong); });
        plugin.with_casted<rotor::internal::registry_plugin_t>([&](auto &p) {
            p.discover_name(ponger_name, ponger_addr).link(true, [&](auto &ec) {
                std::cout << "discovered & linked with ponger : " << (!ec ? "yes" : "no") << "\n";
            });
        });
    }

    void on_start() noexcept override {
        rotor::actor_base_t::on_start();
        std::cout << "lets send ping\n";
        request<payload::ping_t>(ponger_addr).send(timeout);
    }

    void on_pong(message::pong_t &) noexcept {
        std::cout << "pong received, going to shutdown\n";
        supervisor->do_shutdown();
    }

    std::uint32_t attempts = 3;
    rotor::address_ptr_t ponger_addr;
};

struct ponger_t : public rotor::actor_base_t {

    using rotor::actor_base_t::actor_base_t;

    void set_registry_addr(const rotor::address_ptr_t &addr) { registry_addr = addr; }

    void configure(rotor::plugin_t &plugin) noexcept override {
        rotor::actor_base_t::configure(plugin);
        plugin.with_casted<rotor::internal::starter_plugin_t>([](auto &p) { p.subscribe_actor(&ponger_t::on_ping); });
        plugin.with_casted<rotor::internal::registry_plugin_t>([&](auto &p) { p.register_name(ponger_name, address); });
    }

    void on_ping(message::ping_t &req) noexcept {
        std::cout << "ponger recevied ping request\n";
        reply_to(req);
    }

    rotor::address_ptr_t registry_addr;
};

int main() {
    try {
        auto *loop = ev_loop_new(0);
        auto system_context = rotor::ev::system_context_ev_t::ptr_t{new rotor::ev::system_context_ev_t()};
        auto timeout = boost::posix_time::milliseconds{10};

        auto sup = system_context->create_supervisor<rotor::ev::supervisor_ev_t>()
                       .loop(loop)
                       .loop_ownership(true) /* let supervisor takes ownership on the loop */
                       .timeout(timeout)
                       .create_registry(true)
                       .finish();
        auto ponger = sup->create_actor<ponger_t>().timeout(timeout).finish();
        auto pinger = sup->create_actor<pinger_t>().timeout(timeout).finish();

        sup->start();
        ev_run(loop);

    } catch (const std::exception &ex) {
        std::cout << "exception : " << ex.what();
    }

    std::cout << "exiting...\n";
    return 0;
}

/*

sample output:

discovered & linked with ponger : yes
lets send ping
ponger recevied ping request
pong received, going to shutdown
exiting...

*/
