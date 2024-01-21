//
// Copyright (c) 2021-2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <rotor/ev.hpp>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <cstdlib>
#include <thread>
#include <boost/asio/detail/winsock_init.hpp> // for calling WSAStartup on Windows

struct ping_t {};
struct pong_t {};

struct pinger_t : public rotor::actor_base_t {
    using timepoint_t = std::chrono::time_point<std::chrono::high_resolution_clock>;

    using rotor::actor_base_t::actor_base_t;

    void set_pings(std::size_t pings) { pings_left = pings_count = pings; }

    void configure(rotor::plugin::plugin_base_t &plugin) noexcept override {
        rotor::actor_base_t::configure(plugin);
        plugin.with_casted<rotor::plugin::starter_plugin_t>([](auto &p) {
            std::cout << "pinger_t::configure, going to subscribe on_pong\n";
            p.subscribe_actor(&pinger_t::on_pong);
        });
        plugin.with_casted<rotor::plugin::registry_plugin_t>(
            [&](auto &p) { p.discover_name("ponger", ponger_addr, true).link(); });
    }

    void on_start() noexcept override {
        rotor::actor_base_t::on_start();
        std::cout << "pings start (" << pings_left << ")\n";
        start = std::chrono::high_resolution_clock::now();
        send_ping();
    }

    void on_pong(rotor::message_t<pong_t> &) noexcept {
        // std::cout << "pinger_t::on_pong\n";
        send_ping();
    }

  private:
    void send_ping() {
        if (pings_left) {
            send<ping_t>(ponger_addr);
            --pings_left;
        } else {
            using namespace std::chrono;
            auto end = high_resolution_clock::now();
            std::chrono::duration<double> diff = end - start;
            double freq = ((double)pings_count) / diff.count();
            std::cout << "pings finishes (" << pings_left << ") in " << diff.count() << "s"
                      << ", freq = " << std::fixed << std::setprecision(10) << freq << ", real freq = " << std::fixed
                      << std::setprecision(10) << freq * 2 << "\n";
            ponger_addr->supervisor.shutdown();
        }
    }

    timepoint_t start;
    rotor::address_ptr_t ponger_addr;
    std::size_t pings_left;
    std::size_t pings_count;
};

struct ponger_t : public rotor::actor_base_t {

    using rotor::actor_base_t::actor_base_t;

    void set_pinger_addr(const rotor::address_ptr_t &addr) { pinger_addr = addr; }

    void configure(rotor::plugin::plugin_base_t &plugin) noexcept override {
        rotor::actor_base_t::configure(plugin);
        plugin.with_casted<rotor::plugin::starter_plugin_t>([](auto &p) {
            std::cout << "ponger_t::configure, going to subscribe on_ping\n";
            p.subscribe_actor(&ponger_t::on_ping);
        });
        plugin.with_casted<rotor::plugin::registry_plugin_t>(
            [&](auto &p) { p.register_name("ponger", get_address()); });
    }

    void on_ping(rotor::message_t<ping_t> &) noexcept { send<pong_t>(pinger_addr); }

  private:
    rotor::address_ptr_t pinger_addr;
};

int main(int argc, char **argv) {
    try {
        std::uint32_t count = 10000;
        if (argc > 1) {
            count = static_cast<std::uint32_t>(std::atoi(argv[1]));
        }

        auto timeout = boost::posix_time::milliseconds{100};

        auto *loop1 = ev_loop_new(0);
        auto ctx1 = rotor::ev::system_context_ev_t();
        auto sup1 = ctx1.create_supervisor<rotor::ev::supervisor_ev_t>()
                        .loop(loop1)
                        .loop_ownership(true) /* let supervisor takes ownership on the loop */
                        .timeout(timeout)
                        .create_registry()
                        .finish();

        auto *loop2 = ev_loop_new(0);
        auto ctx2 = rotor::ev::system_context_ev_t();
        auto sup2 = ctx2.create_supervisor<rotor::ev::supervisor_ev_t>()
                        .loop(loop2)
                        .loop_ownership(true) /* let supervisor takes ownership on the loop */
                        .timeout(timeout)
                        .registry_address(sup1->get_registry_address())
                        .finish();

        auto pinger = sup1->create_actor<pinger_t>().timeout(timeout).autoshutdown_supervisor().finish();
        auto ponger = sup2->create_actor<ponger_t>().timeout(timeout).finish();
        ponger->set_pinger_addr(pinger->get_address());
        pinger->set_pings(count);

        sup1->start();
        sup2->start();

        auto thread_aux = std::thread([&] { ev_run(loop2); });
        ev_run(loop1);

        sup2->shutdown();
        thread_aux.join();
    } catch (const std::exception &ex) {
        std::cout << "exception : " << ex.what();
    }

    std::cout << "exiting...\n";
    return 0;
}
