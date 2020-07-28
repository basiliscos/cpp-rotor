//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <rotor/ev.hpp>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <cstdlib>

struct ping_t {};
struct pong_t {};

struct pinger_t : public rotor::actor_base_t {
    using timepoint_t = std::chrono::time_point<std::chrono::high_resolution_clock>;

    using rotor::actor_base_t::actor_base_t;

    void set_pings(std::size_t pings) { pings_left = pings_count = pings; }

    void set_ponger_addr(const rotor::address_ptr_t &addr) { ponger_addr = addr; }

    void configure(rotor::plugin_base_t &plugin) noexcept override {
        rotor::actor_base_t::configure(plugin);
        plugin.with_casted<rotor::plugin::starter_plugin_t>([](auto &p) {
            std::cout << "pinger_t::configure, going to subscribe on_pong\n";
            p.subscribe_actor(&pinger_t::on_pong);
        });
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
            supervisor->shutdown();
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

    void configure(rotor::plugin_base_t &plugin) noexcept override {
        rotor::actor_base_t::configure(plugin);
        plugin.with_casted<rotor::plugin::starter_plugin_t>([](auto &p) {
            std::cout << "ponger_t::configure, going to subscribe on_ping\n";
            p.subscribe_actor(&ponger_t::on_ping);
        });
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

        auto *loop = ev_loop_new(0);
        auto system_context = rotor::ev::system_context_ev_t::ptr_t{new rotor::ev::system_context_ev_t()};
        auto timeout = boost::posix_time::milliseconds{10};
        auto sup = system_context->create_supervisor<rotor::ev::supervisor_ev_t>()
                       .loop(loop)
                       .loop_ownership(true) /* let supervisor takes ownership on the loop */
                       .timeout(timeout)
                       .finish();

        auto pinger = sup->create_actor<pinger_t>().timeout(timeout).finish();
        auto ponger = sup->create_actor<ponger_t>().timeout(timeout).finish();
        pinger->set_ponger_addr(ponger->get_address());
        ponger->set_pinger_addr(pinger->get_address());
        pinger->set_pings(count);

        sup->start();
        ev_run(loop);
    } catch (const std::exception &ex) {
        std::cout << "exception : " << ex.what();
    }

    std::cout << "exiting...\n";
    return 0;
}
