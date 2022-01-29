//
// Copyright (c) 2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "actor_test.h"
#include "supervisor_test.h"
#include "system_context_test.h"
#include "access.h"

namespace r = rotor;
namespace rt = r::test;

TEST_CASE("throw in factory", "[spawner]") {

    r::system_context_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::OPERATIONAL);

    size_t invocations = 0;
    SECTION("restart_policy: never") {
        auto factory = [&](r::supervisor_t &, const r::address_ptr_t &) -> r::actor_ptr_t {
            ++invocations;
            throw std::runtime_error("does-not-matter");
        };
        sup->spawn(factory).restart_policy(r::restart_policy_t::never).spawn();
        sup->do_process();
        CHECK(invocations == 1);
    }

    SECTION("restart_policy: normal_only") {
        auto factory = [&](r::supervisor_t &, const r::address_ptr_t &) -> r::actor_ptr_t {
            ++invocations;
            throw std::runtime_error("does-not-matter");
        };
        sup->spawn(factory).restart_policy(r::restart_policy_t::normal_only).spawn();
        sup->do_process();
        CHECK(invocations == 1);
    }

    SECTION("restart_policy: failure_only") {
        auto factory = [&](r::supervisor_t &, const r::address_ptr_t &) -> r::actor_ptr_t {
            ++invocations;
            if (invocations > 3) {
                sup->do_shutdown();
            }
            throw std::runtime_error("does-not-matter");
        };
        sup->spawn(factory).restart_policy(r::restart_policy_t::fail_only).spawn();

        sup->do_process();
        while (!sup->active_timers.empty()) {
            auto &handler = *sup->active_timers.begin();
            sup->do_invoke_timer(handler->request_id);
            sup->do_process();
        }
        CHECK(invocations > 3);
    }

    SECTION("restart_policy: always") {
        auto factory = [&](r::supervisor_t &, const r::address_ptr_t &) -> r::actor_ptr_t {
            ++invocations;
            if (invocations > 3) {
                sup->do_shutdown();
            }
            throw std::runtime_error("does-not-matter");
        };
        sup->spawn(factory).restart_policy(r::restart_policy_t::always).spawn();

        sup->do_process();
        while (!sup->active_timers.empty()) {
            auto &handler = *sup->active_timers.begin();
            sup->do_invoke_timer(handler->request_id);
            sup->do_process();
        }
        CHECK(invocations > 3);
    }

    SECTION("restart_policy: always + max_attempts") {
        auto factory = [&](r::supervisor_t &, const r::address_ptr_t &) -> r::actor_ptr_t {
            ++invocations;
            throw std::runtime_error("does-not-matter");
        };
        sup->spawn(factory).restart_policy(r::restart_policy_t::always).max_attempts(3).spawn();

        sup->do_process();
        while (!sup->active_timers.empty()) {
            auto &handler = *sup->active_timers.begin();
            sup->do_invoke_timer(handler->request_id);
            sup->do_process();
        }
        CHECK(invocations == 3);
    }

    sup->do_shutdown();
    sup->do_process();
    CHECK(sup->get_state() == r::state_t::SHUT_DOWN);
}
