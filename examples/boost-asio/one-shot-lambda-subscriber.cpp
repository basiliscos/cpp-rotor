//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
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

namespace asio = boost::asio;
namespace pt = boost::posix_time;
namespace ra = rotor::asio;

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

struct pinger_t : public rotor::actor_base_t {
    using timepoint_t = std::chrono::time_point<std::chrono::high_resolution_clock>;

    using rotor::actor_base_t::actor_base_t;

    void set_ponger_addr(const rotor::address_ptr_t &addr) { ponger_addr = addr; }

    void configure(rotor::plugin::plugin_base_t &plugin) noexcept override {
        rotor::actor_base_t::configure(plugin);
        plugin.with_casted<rotor::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&pinger_t::on_pong); });
    }

    void on_start() noexcept override {
        rotor::actor_base_t::on_start();
        std::cout << "ping (1)\n";
        auto timeout = rotor::pt::milliseconds{1};
        request<payload::ping_t>(ponger_addr).send(timeout);
    }

    void on_pong(message::pong_t &msg) noexcept {
        auto &ec = msg.payload.ec;
        if (ec) {
            std::cout << "pong error: " << ec.message() << "\n";
            supervisor->do_shutdown();
        } else {
            std::cout << "ping (2)\n";
            auto timeout = rotor::pt::milliseconds{1};
            request<payload::ping_t>(ponger_addr).send(timeout);
        }
    }

    rotor::address_ptr_t ponger_addr;
};

struct ponger_t : public rotor::actor_base_t {
    rotor::handler_ptr_t pong_handler;

    using rotor::actor_base_t::actor_base_t;

    void set_pinger_addr(const rotor::address_ptr_t &addr) { pinger_addr = addr; }

    void configure(rotor::plugin::plugin_base_t &plugin) noexcept override {
        rotor::actor_base_t::configure(plugin);
        plugin.with_casted<rotor::plugin::starter_plugin_t>([&](auto &p) {
            auto lambda = rotor::lambda<message::ping_t>([&](auto &msg) {
                std::cout << "pong\n";
                unsubscribe(pong_handler);
                reply_to(msg);
            });
            rotor::subscription_info_ptr_t info = p.subscribe_actor(std::move(lambda));
            pong_handler = info->handler;
        });
    }

    void shutdown_finish() noexcept override {
        rotor::actor_base_t::shutdown_finish();
        pong_handler.reset(); // otherwise it will be memory leak
    }

  private:
    rotor::address_ptr_t pinger_addr;
};

int main() {
    asio::io_context io_context{1};
    try {

        auto system_context = ra::system_context_asio_t::ptr_t{new ra::system_context_asio_t(io_context)};
        auto strand = std::make_shared<asio::io_context::strand>(io_context);
        auto timeout = boost::posix_time::milliseconds{10};
        auto supervisor =
            system_context->create_supervisor<ra::supervisor_asio_t>().strand(strand).timeout(timeout).finish();

        auto pinger = supervisor->create_actor<pinger_t>().timeout(timeout).finish();
        auto ponger = supervisor->create_actor<ponger_t>().timeout(timeout).finish();
        pinger->set_ponger_addr(ponger->get_address());
        ponger->set_pinger_addr(pinger->get_address());

        supervisor->start();
        io_context.run();
    } catch (const std::exception &ex) {
        std::cout << "exception : " << ex.what();
    }

    std::cout << "exiting...\n";
    return 0;
}
