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
#include <random>
#include <type_traits>
#include <utility>
#include <optional>
#include <unordered_map>

namespace asio = boost::asio;
namespace pt = boost::posix_time;
namespace ra = rotor::asio;

namespace constants {
static float win_probability = 0.97f;
static pt::time_duration ping_timeout = pt::milliseconds{100};
static pt::time_duration ping_reply_base = pt::milliseconds{50};
static pt::time_duration check_interval = pt::milliseconds{3000};
static std::uint32_t ping_reply_scale = 70;
} // namespace constants

namespace resource {
rotor::plugin::resource_id_t timer = 0;
rotor::plugin::resource_id_t ping = 1;
} // namespace resource

namespace payload {
struct pong_t {};
struct ping_t {
    using response_t = pong_t;
};
} // namespace payload

namespace message {
using ping_t = rotor::request_traits_t<payload::ping_t>::request::message_t;
using pong_t = rotor::request_traits_t<payload::ping_t>::response::message_t;
using cancel_t = rotor::request_traits_t<payload::ping_t>::cancel::message_t;
} // namespace message

struct pinger_t : public rotor::actor_base_t {

    using rotor::actor_base_t::actor_base_t;

    void configure(rotor::plugin::plugin_base_t &plugin) noexcept override {
        rotor::actor_base_t::configure(plugin);
        plugin.with_casted<rotor::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&pinger_t::on_pong); });
        plugin.with_casted<rotor::plugin::registry_plugin_t>(
            [&](auto &p) { p.discover_name("ponger", ponger_addr, true).link(); });
    }

    void on_start() noexcept override {
        rotor::actor_base_t::on_start();
        do_ping();

        timer_id = start_timer(constants::check_interval, *this, &pinger_t::on_custom_timeout);
        resources->acquire(resource::timer);
    }

    void do_ping() noexcept {
        resources->acquire(resource::ping);
        request_id = request<payload::ping_t>(ponger_addr).send(constants::ping_timeout);
        ++attempts;
    }

    void on_custom_timeout(rotor::request_id_t, bool cancelled) {
        resources->release(resource::timer);
        timer_id.reset();
        std::cout << "pinger_t, on_custom_timeout, cancelled: " << cancelled << "\n";
        if (!cancelled) {
            do_shutdown();
        }
    }

    void shutdown_start() noexcept override {
        std::cout << "pinger_t, shutdown_start() \n";
        if (request_id)
            send<message::cancel_t>(ponger_addr, get_address());
        if (timer_id)
            cancel_timer(*timer_id);
        rotor::actor_base_t::shutdown_start();
    }

    void shutdown_finish() noexcept override {
        std::cout << "pinger_t, finished attempts done " << attempts << "\n";
        rotor::actor_base_t::shutdown_finish();
    }

    void on_pong(message::pong_t &msg) noexcept {
        resources->release(resource::ping);
        auto &ec = msg.payload.ec;
        if (!ec) {
            std::cout << "success!, pong received, attemps : " << attempts << "\n";
            cancel_timer(*timer_id);
            supervisor->do_shutdown();
        } else {
            std::cout << "pong failed (" << attempts << ")\n";
            if (timer_id) {
                do_ping();
            } else {
                do_shutdown();
            }
        }
    }

    std::optional<rotor::request_id_t> timer_id;
    std::optional<rotor::request_id_t> request_id;
    std::uint32_t attempts = 0;
    rotor::address_ptr_t ponger_addr;
};

struct ponger_t : public rotor::actor_base_t {
    using generator_t = std::mt19937;
    using distrbution_t = std::uniform_real_distribution<double>;
    using message_ptr_t = rotor::intrusive_ptr_t<message::ping_t>;
    using requests_map_t = std::unordered_map<rotor::request_id_t, message_ptr_t>;

    std::random_device rd;
    generator_t gen;
    distrbution_t dist;
    requests_map_t requests;

    explicit ponger_t(config_t &cfg) : rotor::actor_base_t(cfg), gen(rd()) {}

    void configure(rotor::plugin::plugin_base_t &plugin) noexcept override {
        rotor::actor_base_t::configure(plugin);
        plugin.with_casted<rotor::plugin::starter_plugin_t>([](auto &p) {
            p.subscribe_actor(&ponger_t::on_ping);
            p.subscribe_actor(&ponger_t::on_cancel);
        });
        plugin.with_casted<rotor::plugin::registry_plugin_t>(
            [&](auto &p) { p.register_name("ponger", get_address()); });
    }

    void on_ping(message::ping_t &req) noexcept {
        auto dice = constants::ping_reply_scale * dist(gen);
        pt::time_duration reply_after = constants::ping_reply_base + pt::millisec{(int)dice};

        auto timer_id = start_timer(reply_after, *this, &ponger_t::on_ping_timer);
        resources->acquire(resource::timer);
        requests.emplace(timer_id, message_ptr_t(&req));
    }

    void on_cancel(message::cancel_t &notify) noexcept {
        auto request_id = notify.payload.id;
        std::cout << "cancellation notify\n";
        for (auto &it : requests) {
            if (it.second->payload.id == request_id) {
                cancel_timer(it.first);
            }
        }
    }

    void on_ping_timer(rotor::request_id_t timer_id, bool cancelled) noexcept {
        resources->release(resource::timer);
        auto it = requests.find(timer_id);
        if (!cancelled) {
            auto dice = dist(gen);
            if (dice > constants::win_probability) {
                auto &msg = it->second;
                reply_to(*msg);
            }
        } else {
        }
        requests.erase(it);
    }

    void shutdown_start() noexcept override {
        rotor::actor_base_t::shutdown_start();
        for (auto &it : requests) {
            cancel_timer(it.first);
        }
    }

    void shutdown_finish() noexcept override {
        std::cout << "ponger_t, shutdown_finish\n";
        rotor::actor_base_t::shutdown_finish();
    }
};

struct custom_supervisor_t : ra::supervisor_asio_t {
    using ra::supervisor_asio_t::supervisor_asio_t;

    void on_child_shutdown(actor_base_t *, const std::error_code &) noexcept override {
        if (state < rotor::state_t::SHUTTING_DOWN) {
            do_shutdown();
        }
    }

    void shutdown_finish() noexcept override {
        ra::supervisor_asio_t::shutdown_finish();
        strand->context().stop();
    }
};

int main() {
    asio::io_context io_context;
    auto system_context = rotor::asio::system_context_asio_t::ptr_t{new rotor::asio::system_context_asio_t(io_context)};
    auto strand = std::make_shared<asio::io_context::strand>(io_context);
    auto timeout = pt::milliseconds{10};
    auto sup = system_context->create_supervisor<custom_supervisor_t>()
                   .strand(strand)
                   .create_registry()
                   .timeout(timeout)
                   .guard_context(false)
                   .finish();

    sup->create_actor<pinger_t>().timeout(timeout).finish();
    sup->create_actor<ponger_t>().timeout(timeout).finish();

    sup->start();
    io_context.run();
    return 0;
}
