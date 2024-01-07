//
// Copyright (c) 2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

/*
 *
 * This is an example of failure escalation feature, i.e. when an actor
 * terminates with failure, it will trigger its supervisor to shutdown
 * with failure too
 *
 */

#include "rotor.hpp"
#include "dummy_supervisor.h"
#include <iostream>
#include <random>

namespace r = rotor;

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

struct pinger_t : public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;

    void configure(rotor::plugin::plugin_base_t &plugin) noexcept override {
        rotor::actor_base_t::configure(plugin);

        plugin.with_casted<r::plugin::address_maker_plugin_t>([&](auto &p) { p.set_identity("pinger", true); });
        plugin.with_casted<rotor::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&pinger_t::on_pong); });
        plugin.with_casted<rotor::plugin::registry_plugin_t>(
            [&](auto &p) { p.discover_name("ponger", ponger, true).link(); });
    }

    void on_start() noexcept override {
        rotor::actor_base_t::on_start();
        request<payload::ping_t>(ponger).send(init_timeout);
    }

    void on_pong(message::pong_t &reply) noexcept {
        auto &ee = reply.payload.ee;
        if (ee) {
            std::cout << "err: " << ee->message() << "\n";
            return do_shutdown(ee);
        }
        std::cout << "pong received\n";
        do_shutdown();
    }

    rotor::address_ptr_t ponger;
};

struct ponger_t : public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;

    void configure(rotor::plugin::plugin_base_t &plugin) noexcept override {
        rotor::actor_base_t::configure(plugin);
        plugin.with_casted<r::plugin::address_maker_plugin_t>([&](auto &p) { p.set_identity("ponger", true); });
        plugin.with_casted<rotor::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&ponger_t::on_ping); });
        plugin.with_casted<rotor::plugin::registry_plugin_t>(
            [&](auto &p) { p.register_name("ponger", get_address()); });
    }

    void on_ping(message::ping_t &request) noexcept {
        using generator_t = std::mt19937;
        using distribution_t = std::uniform_real_distribution<double>;

        std::cout << "received ping\n";
        std::random_device rd;
        generator_t gen(rd());
        distribution_t dist;

        auto dice = dist(gen);
        std::cout << "pong, dice = " << dice << std::endl;
        if (dice > 0.5) {
            reply_to(request);
        } else {
            auto ec = r::make_error_code(r::error_code_t::request_timeout);
            auto ee = make_error(ec);
            reply_with_error(request, ee);
        }
    }
};

int main() {
    rotor::system_context_t ctx{};
    auto timeout = boost::posix_time::milliseconds{500}; /* does not matter */
    auto sup = ctx.create_supervisor<dummy_supervisor_t>().timeout(timeout).create_registry().finish();
    sup->create_actor<pinger_t>()
        .timeout(timeout)
        .escalate_failure()        /* will shut down in error case */
        .autoshutdown_supervisor() /* will shut down in any case */
        .finish();
    sup->create_actor<ponger_t>().timeout(timeout).finish();
    sup->do_process();

    std::cout << "terminated, reason " << sup->get_shutdown_reason()->message() << "\n";

    return 0;
}

#if 0
sample output 1

received ping
pong, dice = 0.53771
pong received
terminated, reason pinger 0x55a65bc99670 supervisor shutdown due to child shutdown policy
    <- supervisor 0x55a65bc91380 actor shutdown has been requested by supervisor
    <- pinger 0x55a65bc99670 normal shutdown


sample output 2
received ping
pong, dice = 0.271187
err: ponger 0x55a53fcf95f0 request timeout
terminated, reason pinger 0x55a53fcf7670 failure escalation
    <- supervisor 0x55a53fcef380 actor shutdown has been requested by supervisor
    <- ponger 0x55a53fcf95f0 request timeout

#endif
