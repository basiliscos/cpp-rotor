//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

/*
 * This is a little bit simplified example, as we don't supervise actor's synchonization.
 * In the real-world application the proper actions will be:
 * 1. pinger should wait ponger start, and only after that, start pinging it.
 * 2. after finishing it's job, pinger should trigger ponger shutdown
 * 3. pinger should wait ponger shutdown confirmation
 * 4. pinger should release(reset) ponger's address and initiate it's own supervisor shutdown.
 *
 * If (1) is ommitted, then a few messages(pings) might be lost. If (2)..(4) are omitted, then
 * it might lead to memory leak.
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

struct ping_t {};
struct pong_t {};

struct pinger_t : public rotor::actor_base_t {
    using timepoint_t = std::chrono::time_point<std::chrono::high_resolution_clock>;

    pinger_t(rotor::supervisor_t &sup, std::size_t pings)
        : rotor::actor_base_t{sup}, pings_left{pings}, pings_count{pings} {}

    void set_ponger_addr(const rotor::address_ptr_t &addr) { ponger_addr = addr; }

    void on_initialize(rotor::message_t<rotor::payload::initialize_actor_t> &msg) noexcept override {
        rotor::actor_base_t::on_initialize(msg);
        std::cout << "pinger_t::on_initialize\n";
        subscribe(&pinger_t::on_pong);
    }

    void on_start(rotor::message_t<rotor::payload::start_actor_t> &) noexcept override {
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
            supervisor.shutdown();
            ponger_addr.reset(); // do not hold reference to to ponger's supervisor
            // send<rotor::payload::shutdown_request_t>(ponger_addr->supervisor.get_address(), address);
        }
    }

    timepoint_t start;
    rotor::address_ptr_t ponger_addr;
    std::size_t pings_left;
    std::size_t pings_count;
};

struct ponger_t : public rotor::actor_base_t {

    ponger_t(rotor::supervisor_t &sup) : rotor::actor_base_t{sup} {}

    void set_pinger_addr(const rotor::address_ptr_t &addr) { pinger_addr = addr; }

    void on_initialize(rotor::message_t<rotor::payload::initialize_actor_t> &msg) noexcept override {
        rotor::actor_base_t::on_initialize(msg);
        std::cout << "ponger_t::on_initialize\n";
        subscribe(&ponger_t::on_ping);
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

        asio::io_context io_ctx;
        auto asio_guard = asio::make_work_guard(io_ctx);
        auto sys_ctx_asio = rotor::asio::system_context_asio_t::ptr_t{new rotor::asio::system_context_asio_t(io_ctx)};
        auto stand = std::make_shared<asio::io_context::strand>(io_ctx);
        rotor::asio::supervisor_config_t conf_asio{std::move(stand), boost::posix_time::milliseconds{500}};

        auto *loop = ev_loop_new(0);
        auto sys_ctx_ev = rotor::ev::system_context_ev_t::ptr_t{new rotor::ev::system_context_ev_t()};
        auto conf_ev = rotor::ev::supervisor_config_t{
            loop, true, /* let supervisor takes ownership on the loop */
            1.0         /* shutdown timeout */
        };

        auto sup_ev = sys_ctx_ev->create_supervisor<rotor::ev::supervisor_ev_t>(conf_ev);
        auto sup_asio = sys_ctx_asio->create_supervisor<rotor::asio::supervisor_asio_t>(conf_asio);

        auto pinger = sup_ev->create_actor<pinger_t>(count);
        auto ponger = sup_asio->create_actor<ponger_t>();
        pinger->set_ponger_addr(ponger->get_address());
        ponger->set_pinger_addr(pinger->get_address());

        sup_asio->start();
        sup_ev->start();

        auto thread_asio = std::thread([&] { io_ctx.run(); });

        ev_run(loop);

        sup_asio->shutdown();
        asio_guard.reset();
        thread_asio.join();

        std::cout << "main execution complete\n";
        sup_asio->do_process();
        sup_ev->do_process();

        std::cout << "finalization complete\n";
    } catch (const std::exception &ex) {
        std::cout << "exception : " << ex.what();
    }

    std::cout << "exiting...\n";
    return 0;
}
