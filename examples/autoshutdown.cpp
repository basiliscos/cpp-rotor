//
// Copyright (c) 2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

// This example is identical to hello_loopless, except that it demonstrates
// how to autoshutdown supervisor when an actor is down

#include "rotor.hpp"
#include "dummy_supervisor.h"
#include <iostream>

struct hello_actor : public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;
    void on_start() noexcept override {
        rotor::actor_base_t::on_start();
        std::cout << "hello world\n";
        // no need of doing supervisor->do_shutdown()
        do_shutdown();
    }
};

int main() {
    rotor::system_context_t ctx{};
    auto timeout = boost::posix_time::milliseconds{500}; /* does not matter */
    auto sup = ctx.create_supervisor<dummy_supervisor_t>().timeout(timeout).finish();
    sup->create_actor<hello_actor>().timeout(timeout).autoshutdown_supervisor().finish();
    sup->do_process();
    return 0;
}
