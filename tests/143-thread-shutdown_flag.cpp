//
// Copyright (c) 2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#ifdef __unix__
#include "rotor.hpp"
#include "rotor/thread.hpp"
#include "actor_test.h"
#include "supervisor_test.h"
#include "system_context_test.h"
#include "access.h"

#include <unistd.h>
#include <signal.h>
#include <atomic>

namespace r = rotor;
namespace rt = r::test;
namespace rth = rotor::thread;

namespace payload {
struct sample_payload_t {};
} // namespace payload

namespace message {
using sample_payload_t = r::message_t<payload::sample_payload_t>;
}

std::atomic_bool my_flag{false};

TEST_CASE("throw in factory", "[spawner]") {
    auto system_context = rth::system_context_thread_t();
    auto timeout = r::pt::milliseconds{100};
    auto sup = system_context.create_supervisor<rth::supervisor_thread_t>()
                   .timeout(timeout)
                   .shutdown_flag(my_flag, r::pt::millisec{1})
                   .finish();

    auto r = signal(SIGALRM, [](int) { my_flag = true; });
    REQUIRE(r != SIG_ERR);

    alarm(1);

    sup->start();
    system_context.run();

    CHECK(my_flag == true);
    REQUIRE(static_cast<r::actor_base_t *>(sup.get())->access<rt::to::state>() == r::state_t::SHUT_DOWN);
}

#endif
