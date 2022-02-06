//
// Copyright (c) 2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

/*
 * This is an example how to use spawner for simple ping-pong cases.
 * The idea is simple: every time it pinger fails to receive pong, shuts self down
 * and it's spawner spawns new pinger instance and so on until successful
 * pong reply
 *
 */

#include "rotor.hpp"
#include "rotor/thread.hpp"
#include <random>
#include <iostream>

namespace r = rotor;
namespace rth = rotor::thread;
namespace pt = boost::posix_time;

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

        plugin.with_casted<r::plugin::address_maker_plugin_t>([&](auto &p) {
            static int generation = 0;
            std::string id = "pinger #";
            id += std::to_string(++generation);
            p.set_identity(id, true);
        });
        plugin.with_casted<rotor::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&pinger_t::on_pong); });
        plugin.with_casted<rotor::plugin::registry_plugin_t>(
            [&](auto &p) { p.discover_name("ponger", ponger, true).link(); });
    }

    void on_start() noexcept override {
        rotor::actor_base_t::on_start();
        request<payload::ping_t>(ponger).send(init_timeout);
        std::cout << "ping (" << identity << ")\n";
    }

    void on_pong(message::pong_t &reply) noexcept {
        auto &ee = reply.payload.ee;
        if (ee) {
            std::cout << "err: " << ee->message() << "\n";
            return do_shutdown(ee);
        }
        std::cout << "pong received\n";
        // case: succesuful ping. shutdown supervisor. this->do_shutdown() is not enough, as
        // we are managed by spawner policy
        supervisor->do_shutdown();
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
        using distrbution_t = std::uniform_real_distribution<double>;

        std::random_device rd;
        generator_t gen(rd());
        distrbution_t dist;

        auto dice = dist(gen);
        auto failure_probability = 0.925;
        // auto failure_probability = 0.925;
        bool ok = dice > failure_probability;
        std::cout << "pong, dice = " << dice << ", passes threshold : " << (ok ? "yes" : "no") << "\n";
        if (ok) {
            reply_to(request);
        } else {
            auto ec = r::make_error_code(r::error_code_t::request_timeout);
            auto ee = make_error(ec);
            reply_with_error(request, ee);
        }
    }
};

int main(int, char **) {
    rth::system_context_thread_t ctx;
    auto timeout = pt::milliseconds{20};
    auto sup = ctx.create_supervisor<rth::supervisor_thread_t>().timeout(timeout).create_registry().finish();
    sup->create_actor<ponger_t>().timeout(timeout).finish();
    auto pinger_factory = [&](r::supervisor_t &sup, const r::address_ptr_t &spawner) -> r::actor_ptr_t {
        return sup.create_actor<pinger_t>().timeout(timeout).spawner_address(spawner).finish();
    };
    sup->spawn(pinger_factory)
        .max_attempts(15) /* don't do that endlessly */
        .restart_period(timeout)
        .restart_policy(r::restart_policy_t::fail_only) /* case: respawn on single ping fail */
        .escalate_failure()                             /* case: when all pings fail */
        .spawn();

    ctx.run();

    std::cout << "shutdown reason: " << sup->get_shutdown_reason()->message() << "\n";
    return 0;
}

/*

  sample output

ping (pinger #1 0x55d3b09ab130)
pong, dice = 0.000809254, passes threshold : no
err: ponger 0x55d3b09abe80 request timeout
ping (pinger #2 0x55d3b09af090)
pong, dice = 0.446941, passes threshold : no
err: ponger 0x55d3b09abe80 request timeout
ping (pinger #3 0x55d3b09ae670)
pong, dice = 0.809191, passes threshold : no
err: ponger 0x55d3b09abe80 request timeout
ping (pinger #4 0x55d3b09adfd0)
pong, dice = 0.955792, passes threshold : yes
pong received
shutdown reason: supervisor 0x55d3b09a4350 normal shutdown

*/
