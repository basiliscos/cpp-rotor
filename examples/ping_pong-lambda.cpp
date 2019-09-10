//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor.hpp"
#include <iostream>

namespace payload {

struct ping_t {};
struct pong_t {};

} // namespace payload

namespace message {
using ping_t = rotor::message_t<payload::ping_t>;
using pong_t = rotor::message_t<payload::pong_t>;
} // namespace message

struct lambda_pinger_t : public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;

    void set_ponger_addr(const rotor::address_ptr_t &addr) { ponger_addr = addr; }

    void init_start() noexcept override {
        auto handler = rotor::lambda<message::pong_t>([this](auto &) noexcept {
            std::cout << "pong\n";
            supervisor.do_shutdown(); // optional
        });
        subscribe(std::move(handler));
        rotor::actor_base_t::init_start();
    }

    void on_start(rotor::message_t<rotor::payload::start_actor_t> &msg) noexcept override {
        rotor::actor_base_t::on_start(msg);
        send<payload::ping_t>(ponger_addr);
    }

    rotor::address_ptr_t ponger_addr;
};

struct ponger_t : public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;
    void set_pinger_addr(const rotor::address_ptr_t &addr) { pinger_addr = addr; }

    void init_start() noexcept override {
        subscribe(&ponger_t::on_ping);
        rotor::actor_base_t::init_start();
    }

    void on_ping(message::ping_t &) noexcept {
        std::cout << "ping\n";
        send<payload::pong_t>(pinger_addr);
    }

  private:
    rotor::address_ptr_t pinger_addr;
};

struct dummy_supervisor : public rotor::supervisor_t {
    using rotor::supervisor_t::supervisor_t;

    void start_timer(const rotor::pt::time_duration &, timer_id_t) noexcept override {}
    void cancel_timer(timer_id_t) noexcept override {}
    void start() noexcept override {}
    void shutdown() noexcept override {}
    void enqueue(rotor::message_ptr_t) noexcept override {}
};

int main() {
    rotor::system_context_t ctx{};
    auto timeout = boost::posix_time::milliseconds{500}; /* does not matter */
    rotor::supervisor_config_t cfg{timeout};
    auto sup = ctx.create_supervisor<dummy_supervisor>(nullptr, cfg);

    auto pinger = sup->create_actor<lambda_pinger_t>(timeout);
    auto ponger = sup->create_actor<ponger_t>(timeout);
    pinger->set_ponger_addr(ponger->get_address());
    ponger->set_pinger_addr(pinger->get_address());

    sup->do_process();
    return 0;
}
