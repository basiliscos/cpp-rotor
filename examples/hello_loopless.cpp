//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor.hpp"
#include <iostream>

struct hello_actor : public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;
    void on_start() noexcept override {
        rotor::actor_base_t::on_start();
        std::cout << "hello world\n";
        supervisor->do_shutdown();
    }
};

struct dummy_supervisor : public rotor::supervisor_t {
    using rotor::supervisor_t::supervisor_t;

    void start_timer(const rotor::pt::time_duration &, rotor::request_id_t) noexcept override {}
    void cancel_timer(rotor::request_id_t) noexcept override {}
    void start() noexcept override {}
    void shutdown() noexcept override {}
    void enqueue(rotor::message_ptr_t) noexcept override {}
};

int main() {
    rotor::system_context_t ctx{};
    auto timeout = boost::posix_time::milliseconds{500}; /* does not matter */
    auto sup = ctx.create_supervisor<dummy_supervisor>().timeout(timeout).finish();
    sup->create_actor<hello_actor>().timeout(timeout).finish();
    sup->do_process();
    return 0;
}
