#include "rotor/asio.hpp"
#include <iostream>

struct hello_actor: public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;
    void on_start(rotor::message_t<rotor::payload::start_actor_t> &) noexcept override {
        std::cout << "hello world\n";
        do_shutdown();
    }
};

struct dummy_supervisor: public rotor::supervisor_t {
    void start_shutdown_timer() noexcept override {}
    void cancel_shutdown_timer() noexcept override {}
    void start() noexcept override {}
    void shutdown() noexcept override {}
    void enqueue(rotor::message_ptr_t) noexcept override {}
};


int main() {
    rotor::system_context_t ctx{};
    auto sup = ctx.create_supervisor<dummy_supervisor>();
    sup->create_actor<hello_actor>();
    sup->do_process();
    return 0;
}
