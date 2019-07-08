#include "rotor.hpp"
#include <iostream>

struct ping_t {};
struct pong_t {};

struct pinger_t : public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;

    void set_ponger_addr(const rotor::address_ptr_t &addr) { ponger_addr = addr; }

    void on_initialize(rotor::message_t<rotor::payload::initialize_actor_t> &msg) noexcept override {
        rotor::actor_base_t::on_initialize(msg);
        subscribe(&pinger_t::on_pong);
    }

    void on_start(rotor::message_t<rotor::payload::start_actor_t> &msg) noexcept override {
        rotor::actor_base_t::on_start(msg);
        send<ping_t>(ponger_addr);
    }

    void on_pong(rotor::message_t<pong_t> &) noexcept {
        std::cout << "pong\n";
        supervisor.do_shutdown(); // optional
    }

    rotor::address_ptr_t ponger_addr;
};

struct ponger_t : public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;
    void set_pinger_addr(const rotor::address_ptr_t &addr) { pinger_addr = addr; }

    void on_initialize(rotor::message_t<rotor::payload::initialize_actor_t> &msg) noexcept override {
        rotor::actor_base_t::on_initialize(msg);
        subscribe(&ponger_t::on_ping);
    }

    void on_ping(rotor::message_t<ping_t> &) noexcept {
        std::cout << "ping\n";
        send<pong_t>(pinger_addr);
    }

  private:
    rotor::address_ptr_t pinger_addr;
};

struct dummy_supervisor : public rotor::supervisor_t {
    void start_shutdown_timer() noexcept override {}
    void cancel_shutdown_timer() noexcept override {}
    void start() noexcept override {}
    void shutdown() noexcept override {}
    void enqueue(rotor::message_ptr_t) noexcept override {}
};

int main() {
    rotor::system_context_t ctx{};
    auto sup = ctx.create_supervisor<dummy_supervisor>();

    auto pinger = sup->create_actor<pinger_t>();
    auto ponger = sup->create_actor<ponger_t>();
    pinger->set_ponger_addr(ponger->get_address());
    ponger->set_pinger_addr(pinger->get_address());

    sup->do_process();
    return 0;
}
