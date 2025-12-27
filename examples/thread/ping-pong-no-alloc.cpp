//
// Copyright (c) 2025 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

/*
 * This is an example how to do simple ping-pong (single message) without
 * any message allocations, i.e. just by redirecting single message.
 */

#include "rotor.hpp"
#include "rotor/thread.hpp"
#include <boost/lexical_cast.hpp>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <cstdint>

namespace pt = boost::posix_time;
namespace r = rotor;
namespace rth = rotor::thread;

namespace payload {
struct data_t {
    std::uint64_t counter;
};

} // namespace payload

namespace message {
using ping_t = r::message_t<payload::data_t>;
} // namespace message

struct pinger_t : public r::actor_base_t {
    using timepoint_t = std::chrono::time_point<std::chrono::high_resolution_clock>;

    using r::actor_base_t::actor_base_t;

    std::uint64_t initial_count = 1;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        r::actor_base_t::configure(plugin);
        plugin.with_casted<r::plugin::address_maker_plugin_t>([&](auto &p) {
            pong_addr = p.create_address();
            ping_addr = p.create_address();
        });

        plugin.with_casted<r::plugin::starter_plugin_t>([&](auto &p) {
            p.subscribe_actor(&pinger_t::on_pong, pong_addr);
            p.subscribe_actor(&pinger_t::on_ping, ping_addr);
        });
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        send<payload::data_t>(pong_addr, payload::data_t{initial_count});
        std::cout << "starting pinging\n";
        start = std::chrono::high_resolution_clock::now();
    }

    void on_pong(message::ping_t &message) noexcept {
        --message.payload.counter;
        redirect(&message, ping_addr);
    }
    void on_ping(message::ping_t &message) noexcept {
        if (message.payload.counter) {
            redirect(&message, pong_addr);
        } else {
            using namespace std::chrono;
            auto end = high_resolution_clock::now();
            std::chrono::duration<double> diff = end - start;
            double freq = ((double)initial_count) / diff.count();
            std::cout << "pings finishes (" << initial_count << ") in " << diff.count() << "s"
                      << ", freq = " << std::fixed << std::setprecision(10) << freq << ", real freq = " << std::fixed
                      << std::setprecision(10) << freq * 2 << "\n";
            do_shutdown();
        }
    }

  private:
    timepoint_t start;
    r::address_ptr_t pong_addr;
    r::address_ptr_t ping_addr;
};

int main(int argc, char **argv) {
    try {
        using boost::conversion::try_lexical_convert;
        std::uint32_t count = 10000;
        std::uint32_t poll_us = 100;
        if (argc > 1) {
            try_lexical_convert(argv[1], count);
            if (argc > 2) {
                try_lexical_convert(argv[2], poll_us);
            }
        }

        rth::system_context_thread_t ctx;
        auto timeout = boost::posix_time::milliseconds{100};
        auto sup = ctx.create_supervisor<rth::supervisor_thread_t>()
                       .poll_duration(r::pt::milliseconds{poll_us})
                       .timeout(timeout)
                       .create_registry()
                       .finish();
        auto actor = sup->create_actor<pinger_t>().autoshutdown_supervisor().timeout(timeout).finish();
        actor->initial_count = count;
        ctx.run();
    } catch (const std::exception &ex) {
        std::cout << "exception : " << ex.what();
    }

    std::cout << "exiting...\n";
    return 0;
}
