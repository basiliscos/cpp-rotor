//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

/*
 *
 * The example below works because the lifetimes of supervisor's is known apriory, i.e.
 * sup_ev is shutted down while sup_asio is still operational. As the last don't have
 * more job to do, we initiate it's shutdown. In other words, shutdown is partly
 * controlled outside of actors the the sake of simplification.
 *
 */

#include <boost/asio.hpp>
#include <rotor/asio.hpp>
#include <rotor/ev.hpp>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <cstdlib>
#include <thread>

namespace asio = boost::asio;

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
static const auto timeout = boost::posix_time::milliseconds{500};

struct pinger_t : public rotor::actor_base_t {
    using timepoint_t = std::chrono::time_point<std::chrono::high_resolution_clock>;

    using rotor::actor_base_t::actor_base_t;

    void set_pings(std::size_t pings) {
        pings_left = pings;
        pings_count = pings;
    }

    void configure(rotor::plugin::plugin_base_t &plugin) noexcept override {
        rotor::actor_base_t::configure(plugin);
        plugin.with_casted<rotor::plugin::starter_plugin_t>([](auto &p) {
            std::cout << "pinger_t::configure, subscribing on_pong\n";
            p.subscribe_actor(&pinger_t::on_pong);
        });
        plugin.with_casted<rotor::plugin::registry_plugin_t>([&](auto &p) {
            p.discover_name(ponger_name, ponger_addr).link(true).callback([](auto phase, auto &ec) {
                auto p = (phase == rotor::plugin::registry_plugin_t::phase_t::linking) ? "link" : "discovery";
                std::cout << "executing " << p << " in accordance with ponger : " << (!ec ? "yes" : "no") << "\n";
            });
        });
    }

    void on_start() noexcept override {
        rotor::actor_base_t::on_start();
        std::cout << "pings start (" << pings_left << ")\n";
        start = std::chrono::high_resolution_clock::now();
        send_ping();
    }

    void shutdown_finish() noexcept override {
        rotor::actor_base_t::shutdown_finish();
        std::cout << "pinger_t::shutdown_finish\n";
        supervisor->shutdown();
        ponger_addr.reset(); // do not hold reference to to ponger's supervisor
    }

    void on_pong(message::pong_t &reply) noexcept {
        auto &ee = reply.payload.ee;
        if (ee) {
            std::cout << "pong error: " << ee->message() << "\n";
        }
        // std::cout << "pinger_t::on_pong\n";
        send_ping();
    }

  private:
    void send_ping() {
        if (pings_left) {
            request<payload::ping_t>(ponger_addr).send(timeout);
            --pings_left;
        } else {
            using namespace std::chrono;
            auto end = high_resolution_clock::now();
            std::chrono::duration<double> diff = end - start;
            double freq = ((double)pings_count) / diff.count();
            std::cout << "pings complete (" << pings_left << ") in " << diff.count() << "s"
                      << ", freq = " << std::fixed << std::setprecision(10) << freq << ", real freq = " << std::fixed
                      << std::setprecision(10) << freq * 2 << "\n";
            do_shutdown();
        }
    }

    timepoint_t start;
    rotor::address_ptr_t ponger_addr;
    std::size_t pings_left;
    std::size_t pings_count;
};

struct ponger_t : public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;

    void configure(rotor::plugin::plugin_base_t &plugin) noexcept override {
        rotor::actor_base_t::configure(plugin);
        plugin.with_casted<rotor::plugin::starter_plugin_t>([](auto &p) {
            std::cout << "ponger_t::configure, subscribing on_ping\n";
            p.subscribe_actor(&ponger_t::on_ping);
        });
        plugin.with_casted<rotor::plugin::registry_plugin_t>([&](auto &p) {
            std::cout << "ponger_t::configure, registering name\n";
            p.register_name(ponger_name, address);
        });
    }

    void on_ping(message::ping_t &ping) noexcept { reply_to(ping); }

    void shutdown_finish() noexcept override {
        rotor::actor_base_t::shutdown_finish();
        std::cout << "ponger_t::shutdown_finish\n";
    }
};

int main(int argc, char **argv) {
    try {
        std::uint32_t count = 10000;
        if (argc > 1) {
            count = static_cast<std::uint32_t>(std::atoi(argv[1]));
        }

        asio::io_context io_ctx;
        auto sys_ctx_asio = rotor::asio::system_context_asio_t::ptr_t{new rotor::asio::system_context_asio_t(io_ctx)};
        auto strand = std::make_shared<asio::io_context::strand>(io_ctx);

        auto *loop = ev_loop_new(0);
        auto sys_ctx_ev = rotor::ev::system_context_ptr_t{new rotor::ev::system_context_ev_t()};

        auto sup_asio = sys_ctx_asio->create_supervisor<rotor::asio::supervisor_asio_t>()
                            .strand(strand)
                            .timeout(timeout)
                            .create_registry(true)
                            .guard_context(true)
                            .finish();
        auto sup_ev = sys_ctx_ev->create_supervisor<rotor::ev::supervisor_ev_t>()
                          .loop(loop)
                          .loop_ownership(true) /* let supervisor takes ownership on the loop */
                          .timeout(timeout)
                          .registry_address(sup_asio->get_registry_address())
                          .finish();

        auto pinger = sup_ev->create_actor<pinger_t>().timeout(timeout).finish();
        auto ponger = sup_asio->create_actor<ponger_t>().timeout(timeout).finish();
        pinger->set_pings(count);

        sup_asio->start();
        sup_ev->start();

        // explain why
        auto thread_asio = std::thread([&] { io_ctx.run(); });

        ev_run(loop);

        sup_asio->shutdown();
        thread_asio.join();

        sup_asio->do_process();
        sup_ev->do_process();

        std::cout << "finalization complete\n";
    } catch (const std::exception &ex) {
        std::cout << "exception : " << ex.what();
    }

    std::cout << "exiting...\n";
    return 0;
}
