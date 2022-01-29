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

namespace payload {
struct sample_payload_t {};
} // namespace payload

namespace message {
using sample_payload_t = r::message_t<payload::sample_payload_t>;
}

struct sample_actor_t : rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;

    void on_start() noexcept override {
        rt::actor_test_t::on_start();
        send<payload::sample_payload_t>(supervisor->get_address());
    }
};

struct sample_supervisor_t : rt::supervisor_test_t {
    using rt::supervisor_test_t::supervisor_test_t;

    void trigger_timers_and_process() noexcept {
        do_process();
        while (!active_timers.empty()) {
            auto &handler = *active_timers.begin();
            do_invoke_timer(handler->request_id);
            do_process();
        }
    }
};

TEST_CASE("throw in factory", "[spawner]") {
    r::system_context_t system_context;
    auto sup = system_context.create_supervisor<sample_supervisor_t>().timeout(rt::default_timeout).finish();
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

        sup->trigger_timers_and_process();
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

        sup->trigger_timers_and_process();
        CHECK(invocations > 3);
    }

    SECTION("restart_policy: always + max_attempts") {
        auto factory = [&](r::supervisor_t &, const r::address_ptr_t &) -> r::actor_ptr_t {
            ++invocations;
            throw std::runtime_error("does-not-matter");
        };
        sup->spawn(factory).restart_policy(r::restart_policy_t::always).max_attempts(3).spawn();

        sup->trigger_timers_and_process();
        CHECK(invocations == 3);
    }

    sup->do_shutdown();
    sup->do_process();
    CHECK(sup->get_state() == r::state_t::SHUT_DOWN);
}

TEST_CASE("normal flow", "[spawner]") {
    r::system_context_t system_context;
    auto sup = system_context.create_supervisor<sample_supervisor_t>().timeout(rt::default_timeout).finish();
    size_t samples = 0;
    sup->configurer = [&](auto &, r::plugin::plugin_base_t &plugin) {
        plugin.with_casted<r::plugin::starter_plugin_t>([&](auto &p) {
            p.subscribe_actor(r::lambda<message::sample_payload_t>([&](auto &) noexcept { ++samples; }));
        });
    };

    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::OPERATIONAL);
    size_t invocations = 0;
    r::actor_ptr_t act;
    auto factory = [&](r::supervisor_t &, const r::address_ptr_t &spawner) -> r::actor_ptr_t {
        ++invocations;
        act = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).spawner_address(spawner).finish();
        return act;
    };

    auto ec = r::make_error_code(r::error_code_t::cancelled);
    auto ee = r::make_error("custom", ec);

    SECTION("restart_policy: never") {
        sup->spawn(factory).restart_policy(r::restart_policy_t::never).spawn();
        sup->do_process();
        CHECK(invocations == 1);
        CHECK(samples == 1);

        act->do_shutdown();
        sup->do_process();

        sup->do_shutdown();
        sup->do_process();
        CHECK(invocations == 1);
        CHECK(samples == 1);
    }

    SECTION("restart_policy: always") {
        sup->spawn(factory).restart_policy(r::restart_policy_t::always).max_attempts(2).spawn();
        sup->do_process();
        CHECK(invocations == 1);
        CHECK(samples == 1);

        act->do_shutdown();
        act.reset();
        sup->trigger_timers_and_process();

        CHECK(act);
        CHECK(invocations == 2);
        CHECK(samples == 2);

        sup->do_shutdown();
        sup->do_process();
        CHECK(invocations == 2);
        CHECK(samples == 2);
    }

    SECTION("restart_policy: normal_only") {
        sup->spawn(factory).restart_policy(r::restart_policy_t::normal_only).spawn();
        sup->do_process();
        CHECK(invocations == 1);
        CHECK(samples == 1);

        act->do_shutdown();
        act.reset();
        sup->trigger_timers_and_process();

        CHECK(act);
        CHECK(invocations == 2);
        CHECK(samples == 2);

        act->do_shutdown(ee);
        act.reset();
        sup->trigger_timers_and_process();
        CHECK(!act);
        CHECK(invocations == 2);
        CHECK(samples == 2);

        sup->do_shutdown();
        sup->do_process();
        CHECK(invocations == 2);
        CHECK(samples == 2);
    }

    SECTION("restart_policy: fail_only") {
        sup->spawn(factory).restart_policy(r::restart_policy_t::fail_only).spawn();
        sup->do_process();
        CHECK(invocations == 1);
        CHECK(samples == 1);

        act->do_shutdown(ee);
        act.reset();
        sup->trigger_timers_and_process();

        CHECK(act);
        CHECK(invocations == 2);
        CHECK(samples == 2);

        act->do_shutdown();
        act.reset();
        sup->trigger_timers_and_process();
        CHECK(!act);
        CHECK(invocations == 2);
        CHECK(samples == 2);

        sup->do_shutdown();
        sup->do_process();
        CHECK(invocations == 2);
        CHECK(samples == 2);
    }

    CHECK(sup->get_state() == r::state_t::SHUT_DOWN);
}
