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
    r::message_ptr_t init_message;
    std::uint32_t request_attempts;
    timepoint_t start;
    bool stop = false;

    pinger_t(r::supervisor_t &sup, std::size_t pings) : r::actor_base_t{sup}, pings_left{pings}, pings_count{pings} {
        request_attempts = 0;
    }

    void set_ponger_addr(const r::address_ptr_t &addr) { ponger_addr = addr; }

    void on_initialize(r::message_t<r::payload::initialize_actor_t> &msg) noexcept override {
        init_message = &msg;
        subscribe(&pinger_t::on_pong);
        subscribe(&pinger_t::on_ponger_start, ponger_addr);
        subscribe(&pinger_t::on_state);
        request_ponger_status();
        std::cout << "pinger::on_initialize\n";
    }

    void inline request_ponger_status() noexcept {
        send<r::payload::state_request_t>(ponger_addr->supervisor.get_address(), address, ponger_addr);
    }

    void inline finalize_init() noexcept {
        std::cout << "pinger::finalize_init\n";
        using init_msg_t = r::message_t<r::payload::initialize_actor_t>;
        auto &init_msg = static_cast<init_msg_t &>(*init_message);
        r::actor_base_t::on_initialize(init_msg);
        init_message.reset();
    }

    void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        std::cout << "pinger::on_start\n";
        r::actor_base_t::on_start(msg);
        unsubscribe(&pinger_t::on_ponger_start, ponger_addr);
        unsubscribe(&pinger_t::on_state);
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

    void on_ponger_start(r::message_t<r::payload::start_actor_t> &) noexcept {
        std::cout << "pinger::on_ponger_start\n";
        if (init_message) {
            finalize_init();
        }
        // we already get the right
    }

    void on_state(r::message_t<r::payload::state_response_t> &msg) noexcept {
        auto target_state = msg.payload.state;
        std::cout << "pinger::on_state " << static_cast<int>(target_state) << "\n";
        if (!init_message) {
            return; // we are already  on_ponger_start
        }
        if (target_state == r::state_t::OPERATIONAL) {
            finalize_init();
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
            if (!stop) {
                stop = true;
                using namespace std::chrono;
                auto end = high_resolution_clock::now();
                std::chrono::duration<double> diff = end - start;
                double freq = ((double)pings_count) / diff.count();
                std::cout << "pings finishes (" << pings_left << ") in " << diff.count() << "s"
                          << ", freq = " << std::fixed << std::setprecision(10) << freq
                          << ", real freq = " << std::fixed << std::setprecision(10) << freq * 2 << "\n";
                supervisor.shutdown();
                ponger_addr->supervisor.shutdown();
            }
        }
    }

    r::address_ptr_t ponger_addr;
};

struct ponger_t : public r::actor_base_t {

    ponger_t(r::supervisor_t &sup) : r::actor_base_t{sup} {}

    void set_pinger_addr(const r::address_ptr_t &addr) { pinger_addr = addr; }

    void on_initialize(r::message_t<r::payload::initialize_actor_t> &msg) noexcept override {
        r::actor_base_t::on_initialize(msg);
        subscribe(&ponger_t::on_ping);
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

int main(int argc, char **argv) {
    asio::io_context io_ctx;
    try {
        std::uint32_t count = 10000;
        if (argc > 1) {
            boost::conversion::try_lexical_convert(argv[1], count);
        }

        auto sys_ctx = ra::system_context_asio_t::ptr_t{new ra::system_context_asio_t(io_ctx)};
        ra::supervisor_config_t conf{pt::milliseconds{500}};
        auto sup1 = sys_ctx->create_supervisor<ra::supervisor_asio_t>(conf);
        auto sup2 = sup1->create_actor<ra::supervisor_asio_t>(sys_ctx, conf);

        auto pinger = sup1->create_actor<pinger_t>(count);
        auto ponger = sup2->create_actor<ponger_t>();
        pinger->set_ponger_addr(ponger->get_address());
        ponger->set_pinger_addr(pinger->get_address());

        sup1->start();
        sup2->start();

        auto t1 = std::thread([&] { io_ctx.run(); });
        auto t2 = std::thread([&] { io_ctx.run(); });
        t1.join();
        t2.join();

        std::cout << "pings left: " << pinger->pings_left << "\n";

    } catch (const std::exception &ex) {
        std::cout << "exception : " << ex.what();
    }

    std::cout << "exiting...\n";
    return 0;
}
