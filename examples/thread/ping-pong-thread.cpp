//
// Copyright (c) 2021-2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

/*
 * This is an example how to do simple ping-pong using different threads
 *
 */

#include "rotor.hpp"
#include "rotor/thread.hpp"
#include <boost/lexical_cast.hpp>
#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace pt = boost::posix_time;
namespace r = rotor;
namespace rth = rotor::thread;

namespace payload {
struct ping_t {};
struct pong_t {};
using announce_t = r::address_ptr_t;
} // namespace payload

namespace message {
using ping_t = r::message_t<payload::ping_t>;
using pong_t = r::message_t<payload::pong_t>;
using announce_t = r::message_t<payload::announce_t>;
} // namespace message

struct pinger_t : public r::actor_base_t {
    using timepoint_t = std::chrono::time_point<std::chrono::high_resolution_clock>;

    using r::actor_base_t::actor_base_t;
    void set_pings(std::size_t pings) { pings_left = pings_count = pings; }

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        r::actor_base_t::configure(plugin);
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) {
            std::cout << "pinger_t::configure(), subscribing to on_pong\n";
            p.subscribe_actor(&pinger_t::on_pong);
        });
        plugin.with_casted<rotor::plugin::registry_plugin_t>(
            [&](auto &p) { p.discover_name("ponger", ponger_addr, true).link(true); });
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        send<payload::announce_t>(ponger_addr, get_address());
        std::cout << "pings start (" << pings_left << ")\n";
        start = std::chrono::high_resolution_clock::now();
        send_ping();
    }

    void on_pong(message::pong_t &) noexcept {
        // std::cout << "pinger_t::on_pong\n";
        send_ping();
    }

  private:
    void send_ping() {
        if (pings_left) {
            send<payload::ping_t>(ponger_addr);
            --pings_left;
        } else {
            using namespace std::chrono;
            auto end = high_resolution_clock::now();
            std::chrono::duration<double> diff = end - start;
            double freq = ((double)pings_count) / diff.count();
            std::cout << "pings finishes (" << pings_left << ") in " << diff.count() << "s"
                      << ", freq = " << std::fixed << std::setprecision(10) << freq << ", real freq = " << std::fixed
                      << std::setprecision(10) << freq * 2 << "\n";
            do_shutdown();
        }
    }

    timepoint_t start;
    r::address_ptr_t ponger_addr;
    std::size_t pings_left;
    std::size_t pings_count;
};

struct ponger_t : public r::actor_base_t {

    using r::actor_base_t::actor_base_t;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        r::actor_base_t::configure(plugin);
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) {
            std::cout << "pinger_t::configure, subscribing on_ping\n";
            p.subscribe_actor(&ponger_t::on_announce);
            p.subscribe_actor(&ponger_t::on_ping);
        });
        plugin.with_casted<rotor::plugin::registry_plugin_t>(
            [&](auto &p) { p.register_name("ponger", get_address()); });
    }

    void on_announce(message::announce_t &message) noexcept { pinger_addr = message.payload; }
    void on_ping(message::ping_t &) noexcept { send<payload::pong_t>(pinger_addr); }

  private:
    r::address_ptr_t pinger_addr;
};

int main(int argc, char **argv) {
    try {
        std::uint32_t count = 10000;
        if (argc > 1) {
            boost::conversion::try_lexical_convert(argv[1], count);
        }

        rth::system_context_thread_t ctx_ping;
        rth::system_context_thread_t ctx_pong;
        auto timeout = boost::posix_time::milliseconds{100};
        auto sup_ping =
            ctx_ping.create_supervisor<rth::supervisor_thread_t>().timeout(timeout).create_registry().finish();
        auto pinger = sup_ping->create_actor<pinger_t>().autoshutdown_supervisor().timeout(timeout).finish();
        pinger->set_pings(count);

        auto sup_pong = ctx_pong.create_supervisor<rth::supervisor_thread_t>()
                            .timeout(timeout)
                            .registry_address(sup_ping->get_registry_address())
                            .finish();
        auto ponger = sup_pong->create_actor<ponger_t>().timeout(timeout).finish();

        sup_ping->start();
        sup_pong->start();

        auto pong_thread = std::thread([&] { ctx_pong.run(); });

        ctx_ping.run();
        sup_pong->shutdown();
        pong_thread.join();
    } catch (const std::exception &ex) {
        std::cout << "exception : " << ex.what();
    }

    std::cout << "exiting...\n";
    return 0;
}
