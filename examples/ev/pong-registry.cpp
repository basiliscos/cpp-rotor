//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <rotor/ev.hpp>
#include <rotor/registry.h>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <cstdlib>

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
static const auto timeout = boost::posix_time::milliseconds{5};

struct pinger_t : public rotor::actor_base_t {

    using rotor::actor_base_t::actor_base_t;

    void set_registry_addr(const rotor::address_ptr_t &addr) { registry_addr = addr; }

    void init_start() noexcept override {
        subscribe(&pinger_t::on_discovery);
        subscribe(&pinger_t::on_status);
        subscribe(&pinger_t::on_pong);
        request<rotor::payload::discovery_request_t>(registry_addr, ponger_name).send(timeout);
    }

    void on_discovery(rotor::message::discovery_response_t &msg) noexcept {
        auto &ec = msg.payload.ec;
        if (ec) {
            std::cout << "ponger address wasn't found: " << ec.message() << "\n";
            if (attempts) {
                --attempts;
                std::cout << "lets try to discover ponger address again (" << attempts << " attempts left)\n";
                request<rotor::payload::discovery_request_t>(registry_addr, ponger_name).send(timeout);
            } else {
                supervisor.do_shutdown();
            }
            return;
        }
        unsubscribe(&pinger_t::on_discovery); // optional
        std::cout << "ponger address was succesfully discovered\n";
        ponger_addr = msg.payload.res.service_addr;
        auto ponger_sup_addr = ponger_addr->supervisor.get_address();
        request<rotor::payload::state_request_t>(ponger_sup_addr, ponger_addr).send(timeout);
    }

    void on_status(rotor::message::state_response_t &msg) noexcept {
        auto &ec = msg.payload.ec;
        if (ec) {
            std::cout << "ponger state cannot be determined: " << ec.message() << "\n";
            supervisor.do_shutdown();
            return;
        }
        auto state = msg.payload.res.state;
        if (state == rotor::state_t::OPERATIONAL) {
            std::cout << "ponger state is operational, continue pinger init\n";
            rotor::actor_base_t::init_start();
        } else {
            std::cout << "ponger state " << static_cast<int>(state) << " isnt operational\n";
            if (attempts) {
                --attempts;
                std::cout << "lets try to check the state again (" << attempts << " attempts left)\n";
                auto ponger_sup_addr = ponger_addr->supervisor.get_address();
                request<rotor::payload::state_request_t>(ponger_sup_addr, ponger_addr).send(timeout);
            } else {
                supervisor.do_shutdown();
            }
        }
    }

    void on_start(rotor::message_t<rotor::payload::start_actor_t> &msg) noexcept override {
        std::cout << "lets send ping\n";
        rotor::actor_base_t::on_start(msg);
        request<payload::ping_t>(ponger_addr).send(timeout);
    }

    void on_pong(message::pong_t &) noexcept {
        std::cout << "pong received, going to shutdown\n";
        supervisor.do_shutdown();
    }

    std::uint32_t attempts = 3;
    rotor::address_ptr_t registry_addr;
    rotor::address_ptr_t ponger_addr;
};

struct ponger_t : public rotor::actor_base_t {

    using rotor::actor_base_t::actor_base_t;

    void set_registry_addr(const rotor::address_ptr_t &addr) { registry_addr = addr; }

    void init_start() noexcept override {
        subscribe(&ponger_t::on_ping);
        subscribe(&ponger_t::on_registration);
        request<rotor::payload::registration_request_t>(registry_addr, ponger_name, address).send(timeout);
    }

    void shutdown_start() noexcept override {
        send<rotor::payload::deregistration_notify_t>(registry_addr, address);
        rotor::actor_base_t::shutdown_start();
    }

    void on_registration(rotor::message::registration_response_t &msg) noexcept {
        auto &ec = msg.payload.ec;
        if (ec) {
            std::cout << "ponger registration failure: " << ec.message() << "\n";
            return;
        }
        std::cout << "ponger has been registered, resume initialization \n";
        rotor::actor_base_t::init_start();
    }

    void on_ping(message::ping_t &req) noexcept {
        std::cout << "ponger recevied ping request\n";
        reply_to(req);
    }

    rotor::address_ptr_t registry_addr;
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
        auto registry = sup->create_actor<rotor::registry_t>(timeout);
        auto pinger = sup->create_actor<pinger_t>(timeout);
        auto ponger = sup->create_actor<ponger_t>(timeout);

        ponger->set_registry_addr(registry->get_address());
        pinger->set_registry_addr(registry->get_address());
        sup->start();
        ev_run(loop);

    } catch (const std::exception &ex) {
        std::cout << "exception : " << ex.what();
    }

    std::cout << "exiting...\n";
    return 0;
}

/*

sample output:

ponger address wasn't found: the requested service name is not registered
lets try to discover ponger address again (2 attempts left)
ponger has been registered, resume initialization
ponger address was succesfully discovered
ponger state is operational, continue pinger init
lets send ping
ponger recevied ping request
pong received, going to shutdown
exiting...

*/
