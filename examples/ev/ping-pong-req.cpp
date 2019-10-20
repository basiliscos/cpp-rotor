//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <rotor/ev.hpp>
#include <iostream>
#include <random>

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

    using rotor::actor_base_t::actor_base_t;

    void set_ponger_addr(const rotor::address_ptr_t &addr) { ponger_addr = addr; }

    void on_initialize(rotor::message::init_request_t &msg) noexcept override {
        rotor::actor_base_t::on_initialize(msg);
        subscribe(&pinger_t::on_pong);
    }

    void on_start(rotor::message_t<rotor::payload::start_actor_t> &) noexcept override {
        request<payload::ping_t>(ponger_addr).send(rotor::pt::seconds(1));
    }

    void on_pong(message::pong_t &msg) noexcept {
        auto &ec = msg.payload.ec;
        if (!msg.payload.ec) {
            std::cout << "pong received\n";
        } else {
            std::cout << "pong was NOT received: " << ec.message() << "\n";
        }
        supervisor.do_shutdown();
    }

    rotor::address_ptr_t ponger_addr;
};

struct ponger_t : public rotor::actor_base_t {
    using generator_t = std::mt19937;
    using distrbution_t = std::uniform_real_distribution<double>;

    std::random_device rd;
    generator_t gen;
    distrbution_t dist;

    ponger_t(rotor::supervisor_t &sup) : rotor::actor_base_t{sup}, gen(rd()) {}

    void on_initialize(rotor::message::init_request_t &msg) noexcept override {
        rotor::actor_base_t::on_initialize(msg);
        subscribe(&ponger_t::on_ping);
    }

    void on_ping(message::ping_t &req) noexcept {
        auto dice = dist(gen);
        std::cout << "pong, dice = " << dice << std::endl;
        if (dice > 0.5) {
            reply_to(req);
        }
    }
};

int main() {
    try {
        auto *loop = ev_loop_new(0);
        auto system_context = rotor::ev::system_context_ev_t::ptr_t{new rotor::ev::system_context_ev_t()};
        auto timeout = boost::posix_time::milliseconds{10};
        auto conf = rotor::ev::supervisor_config_ev_t{
            timeout, loop, true, /* let supervisor takes ownership on the loop */
        };
        auto sup = system_context->create_supervisor<rotor::ev::supervisor_ev_t>(conf);

        auto pinger = sup->create_actor<pinger_t>(timeout);
        auto ponger = sup->create_actor<ponger_t>(timeout);
        pinger->set_ponger_addr(ponger->get_address());

        sup->start();
        ev_run(loop);
    } catch (const std::exception &ex) {
        std::cout << "exception : " << ex.what();
    }

    std::cout << "exiting...\n";
    return 0;
}
