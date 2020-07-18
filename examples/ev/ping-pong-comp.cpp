//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <rotor/ev.hpp>
#include <iostream>
#include <random>
#include <unordered_map>

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

struct shared_context_t {
    std::size_t pings_left;
    std::size_t pings_success = 0;
    std::size_t pings_error = 0;
};

struct pinger_t : public rotor::actor_base_t {
    using map_t = std::unordered_map<rotor::address_ptr_t, shared_context_t>;

    using rotor::actor_base_t::actor_base_t;

    void set_ponger_addr1(const rotor::address_ptr_t &addr) { ponger_addr1 = addr; }
    void set_ponger_addr2(const rotor::address_ptr_t &addr) { ponger_addr2 = addr; }

    void configure(rotor::plugin_t &plugin) noexcept override {
        rotor::actor_base_t::configure(plugin);
        plugin.with_casted<rotor::internal::starter_plugin_t>([&](auto &p) {
            if (!reply_addr)
                reply_addr = create_address();
            p.subscribe_actor(&pinger_t::on_pong, reply_addr);
        });
    }

    void on_start() noexcept override {
        rotor::actor_base_t::on_start();
        request_via<payload::ping_t>(ponger_addr1, reply_addr).send(rotor::pt::seconds(1));
        request_via<payload::ping_t>(ponger_addr2, reply_addr).send(rotor::pt::seconds(1));
        request_map.emplace(reply_addr, shared_context_t{2});
    }

    void on_pong(message::pong_t &msg) noexcept {
        auto &ctx = request_map[msg.address];
        --ctx.pings_left;
        auto &ec = msg.payload.ec;
        if (ec) {
            ++ctx.pings_error;
        } else {
            ++ctx.pings_success;
        }
        if (!ctx.pings_left) {
            std::cout << "success: " << ctx.pings_success << ", errors: " << ctx.pings_error << "\n";
            // optional cleanup
            request_map.erase(msg.address);
            supervisor->do_shutdown();
        }
    }

    map_t request_map;
    rotor::address_ptr_t ponger_addr1;
    rotor::address_ptr_t ponger_addr2;
    rotor::address_ptr_t reply_addr;
};

struct ponger_t : public rotor::actor_base_t {
    using generator_t = std::mt19937;
    using distrbution_t = std::uniform_real_distribution<double>;

    std::random_device rd;
    generator_t gen;
    distrbution_t dist;

    ponger_t(config_t &cfg) : rotor::actor_base_t(cfg), gen(rd()) {}

    void configure(rotor::plugin_t &plugin) noexcept override {
        rotor::actor_base_t::configure(plugin);
        plugin.with_casted<rotor::internal::starter_plugin_t>([](auto &p) { p.subscribe_actor(&ponger_t::on_ping); });
    }

    void on_ping(message::ping_t &req) noexcept {
        auto dice = dist(gen);
        std::cout << "pong, dice = " << dice << std::endl;
        if (dice > 0.5) {
            reply_to(req);
        }
    }
};

namespace to {
struct address {};
} // namespace to

namespace rotor {
template <> inline auto &actor_base_t::access<to::address>() noexcept { return address; }
} // namespace rotor

int main() {
    try {
        auto *loop = ev_loop_new(0);
        auto system_context = rotor::ev::system_context_ev_t::ptr_t{new rotor::ev::system_context_ev_t()};
        auto timeout = boost::posix_time::milliseconds{10};
        auto sup = system_context->create_supervisor<rotor::ev::supervisor_ev_t>()
                       .loop(loop)
                       .loop_ownership(true) /* let supervisor takes ownership on the loop */
                       .timeout(timeout)
                       .finish();

        auto pinger = sup->create_actor<pinger_t>().timeout(timeout).finish();
        auto ponger1 = sup->create_actor<ponger_t>().timeout(timeout).finish();
        auto ponger2 = sup->create_actor<ponger_t>().timeout(timeout).finish();

        pinger->set_ponger_addr1(ponger1->access<to::address>());
        pinger->set_ponger_addr2(ponger2->access<to::address>());

        sup->start();
        ev_run(loop);
    } catch (const std::exception &ex) {
        std::cout << "exception : " << ex.what();
    }

    std::cout << "exiting...\n";
    return 0;
}
