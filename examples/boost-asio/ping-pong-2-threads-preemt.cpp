//
// Copyright (c) 2019-2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor.hpp"
#include "rotor/asio.hpp"
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>
#include <thread>

namespace asio = boost::asio;
namespace pt = boost::posix_time;
namespace ra = rotor::asio;
namespace r = rotor;

struct ping_t {};
struct pong_t {};

struct pinger_t : public r::actor_base_t {
    using timepoint_t = std::chrono::time_point<std::chrono::high_resolution_clock>;

    std::size_t pings_left;
    std::size_t pings_count;
    std::uint32_t request_attempts = 0;
    timepoint_t start;
    bool stop = false;

    using r::actor_base_t::actor_base_t;
    void set_ponger_addr(const r::address_ptr_t &addr) { ponger_addr = addr; }
    void set_pings(std::size_t pings) { pings_count = pings_left = pings; }

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<r::plugin::starter_plugin_t>([&](auto &p) { p.subscribe_actor(&pinger_t::on_pong); });
        plugin.with_casted<r::plugin::link_client_plugin_t>([&](auto &p) {
            p.link(ponger_addr, true, [&](auto &ec) {
                if (ec) {
                    std::cout << "error linking pinger with ponger :: " << ec->message() << "\n";
                } else {
                    std::cout << "pinger has been successfully linked with ponger\n";
                }
            });
        });
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        std::cout << "pinger::on_start\n";
        start = std::chrono::high_resolution_clock::now();
        do_send_ping();
        do_send_ping();
        do_send_ping();
        do_send_ping();
        do_send_ping();
        do_send_ping();
        do_send_ping();
        do_send_ping();
        do_send_ping();
        do_send_ping();
    }

    void on_pong(r::message_t<pong_t> &) noexcept { do_send_ping(); }

    void do_send_ping() {
        // std::cout << "pinger::do_send_ping, left: " << pings_left << "\n";
        if (pings_left) {
            send<ping_t>(ponger_addr);
            --pings_left;
        } else {
            if (!stop) {
                stop = true;
                using namespace std::chrono;
                auto end = high_resolution_clock::now();
                std::chrono::duration<double> diff = end - start;
                double freq = ((double)pings_count) / diff.count();
                std::cout << "pings finishes (" << pings_left << ") in " << diff.count() << "s"
                          << ", freq = " << std::fixed << std::setprecision(10) << freq
                          << ", real freq = " << std::fixed << std::setprecision(10) << freq * 2 << "\n";
                ponger_addr->supervisor.shutdown();
            }
        }
    }

    // we need of this to let somebody shutdown everything
    void shutdown_finish() noexcept override {
        r::actor_base_t::shutdown_finish();
        do_shutdown();
        ponger_addr->supervisor.shutdown();
        std::cout << "pinger shutdown finish\n";
    }

    r::address_ptr_t ponger_addr;
};

struct ponger_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;

    void set_pinger_addr(const r::address_ptr_t &addr) { pinger_addr = addr; }

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) {
            std::cout << "ponger::configure, subscribing on_ping\n";
            p.subscribe_actor(&ponger_t::on_ping);
        });
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        std::cout << "ponger::on_start\n";
    }

    void on_ping(r::message_t<ping_t> &) noexcept {
        // std::cout << "ponger::on_ping\n";
        send<pong_t>(pinger_addr);
    }

  private:
    r::address_ptr_t pinger_addr;
};

int main(int argc, char **argv) {
    asio::io_context io_ctx;
    try {
        std::uint32_t count = 100;
        if (argc > 1) {
            boost::conversion::try_lexical_convert(argv[1], count);
        }

        auto sys_ctx = ra::system_context_asio_t::ptr_t{new ra::system_context_asio_t(io_ctx)};
        auto strand1 = std::make_shared<asio::io_context::strand>(io_ctx);
        auto strand2 = std::make_shared<asio::io_context::strand>(io_ctx);
        auto timeout = boost::posix_time::milliseconds{1000};
        auto sup1 = sys_ctx->create_supervisor<ra::supervisor_asio_t>().strand(strand1).timeout(timeout).finish();
        auto sup2 = sup1->create_actor<ra::supervisor_asio_t>().strand(strand2).timeout(timeout).finish();

        // warm-up
        sup1->do_process();
        sup2->do_process();

        auto pinger = sup1->create_actor<pinger_t>().timeout(timeout).autoshutdown_supervisor().finish();
        auto ponger = sup2->create_actor<ponger_t>().timeout(timeout).finish();
        pinger->set_ponger_addr(ponger->get_address());
        pinger->set_pings(count);
        ponger->set_pinger_addr(pinger->get_address());

        auto t1 = std::thread([&] { io_ctx.run(); });
        auto t2 = std::thread([&] { io_ctx.run(); });
        t2.join();

        sup1->shutdown();
        t1.join();

        std::cout << "pings left: " << pinger->pings_left << "\n";

    } catch (const std::exception &ex) {
        std::cout << "exception : " << ex.what();
    }

    std::cout << "exiting...\n";
    return 0;
}
