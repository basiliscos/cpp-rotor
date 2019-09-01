//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
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
    std::uint32_t request_attempts;
    timepoint_t start;

    pinger_t(r::supervisor_t &sup, std::size_t pings) : r::actor_base_t{sup}, pings_left{pings}, pings_count{pings} {
        request_attempts = 0;
    }

    void set_ponger_addr(const r::address_ptr_t &addr) { ponger_addr = addr; }

    void on_initialize(r::message::init_request_t &msg) noexcept override {
        subscribe(&pinger_t::on_pong);
        subscribe(&pinger_t::on_ponger_start, ponger_addr);
        subscribe(&pinger_t::on_state);
        r::actor_base_t::on_initialize(msg);
        std::cout << "pinger::on_initialize\n";
    }

    void init_start() noexcept override { request_ponger_status(); }

    void inline request_ponger_status() noexcept {
        request<r::payload::state_request_t>(ponger_addr->supervisor.get_address(), ponger_addr)
            .timeout(r::pt::millisec{1});
    }

    void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        std::cout << "pinger::on_start\n";
        r::actor_base_t::on_start(msg);
        unsubscribe(&pinger_t::on_ponger_start, ponger_addr);
        unsubscribe(&pinger_t::on_state);
        start = std::chrono::high_resolution_clock::now();
        do_send_ping();
    }

    void on_pong(r::message_t<pong_t> &) noexcept { do_send_ping(); }

    void on_ponger_start(r::message_t<r::payload::start_actor_t> &) noexcept {
        std::cout << "pinger::on_ponger_start\n";
        if (state == r::state_t::INITIALIZING) {
            r::actor_base_t::init_start();
        }
        // we already get the right
    }

    void on_state(r::message::state_response_t &msg) noexcept {
        // IRL we should check for errors in msg.payload.ec and react appropriately
        auto &target_state = msg.payload.res.state;
        std::cout << "pinger::on_state " << static_cast<int>(target_state) << "\n";
        if (state == r::state_t::INITIALIZED) {
            return; // we are already  on_ponger_start
        }
        if (target_state == r::state_t::OPERATIONAL) {
            r::actor_base_t::init_start();
        } else {
            if (request_attempts > 3) {
                // do_shutdown();
            } else {
                request_ponger_status();
            }
        }
    }

    void do_send_ping() {
        // std::cout << "pinger::do_send_ping, left: " << pings_left << "\n";
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
            ponger_addr->supervisor.shutdown();
        }
    }

    r::address_ptr_t ponger_addr;
};

struct ponger_t : public r::actor_base_t {

    ponger_t(r::supervisor_t &sup) : r::actor_base_t{sup} {}

    void set_pinger_addr(const r::address_ptr_t &addr) { pinger_addr = addr; }

    void on_initialize(r::message::init_request_t &msg) noexcept override {
        subscribe(&ponger_t::on_ping);
        r::actor_base_t::on_initialize(msg);
        std::cout << "ponger::on_initialize\n";
    }

    void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        std::cout << "ponger::on_start\n";
        r::actor_base_t::on_start(msg);
    }

    void on_ping(r::message_t<ping_t> &) noexcept {
        // std::cout << "ponger::on_ping\n";
        send<pong_t>(pinger_addr);
    }

  private:
    r::address_ptr_t pinger_addr;
};

struct holding_supervisor_t : public ra::supervisor_asio_t {
    using guard_t = asio::executor_work_guard<asio::io_context::executor_type>;

    holding_supervisor_t(ra::supervisor_asio_t *sup, const ra::supervisor_config_asio_t &cfg)
        : ra::supervisor_asio_t{sup, cfg}, guard{asio::make_work_guard(cfg.strand->context())} {}
    guard_t guard;

    void shutdown_finish() noexcept override {
        ra::supervisor_asio_t::shutdown_finish();
        guard.reset();
        std::cout << "holding_supervisor_t::confirm_shutdown\n";
    }
};

int main(int argc, char **argv) {

    asio::io_context io_ctx1;
    asio::io_context io_ctx2;
    try {
        std::uint32_t count = 10000;
        if (argc > 1) {
            boost::conversion::try_lexical_convert(argv[1], count);
        }

        auto sys_ctx1 = ra::system_context_asio_t::ptr_t{new ra::system_context_asio_t(io_ctx1)};
        auto sys_ctx2 = ra::system_context_asio_t::ptr_t{new ra::system_context_asio_t(io_ctx2)};
        auto stand1 = std::make_shared<asio::io_context::strand>(io_ctx1);
        auto stand2 = std::make_shared<asio::io_context::strand>(io_ctx2);
        auto timeout = boost::posix_time::milliseconds{10};
        ra::supervisor_config_asio_t conf1{timeout, std::move(stand1)};
        ra::supervisor_config_asio_t conf2{timeout, std::move(stand2)};
        auto sup1 = sys_ctx1->create_supervisor<holding_supervisor_t>(conf1);
        auto sup2 = sys_ctx2->create_supervisor<holding_supervisor_t>(conf2);

        auto pinger = sup1->create_actor<pinger_t>(timeout, count);
        auto ponger = sup2->create_actor<ponger_t>(timeout);
        pinger->set_ponger_addr(ponger->get_address());
        ponger->set_pinger_addr(pinger->get_address());

        sup1->start();
        sup2->start();

        auto t1 = std::thread([&] { io_ctx1.run(); });
        auto t2 = std::thread([&] { io_ctx2.run(); });
        t1.join();
        t2.join();

        std::cout << "pings left: " << pinger->pings_left << "\n";

    } catch (const std::exception &ex) {
        std::cout << "exception : " << ex.what();
    }

    std::cout << "exiting...\n";
    return 0;
}
