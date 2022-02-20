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
    bool restart_flag = false;

    void on_start() noexcept override {
        rt::actor_test_t::on_start();
        auto sup = supervisor->access<rt::to::locality_leader>();
        send<payload::sample_payload_t>(sup->get_address());
    }

    bool should_restart() const noexcept override { return restart_flag; }
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

struct test_supervisor1_t : rt::supervisor_test_t {
    using rt::supervisor_test_t::supervisor_test_t;

    void on_start() noexcept override {
        rt::supervisor_test_t::on_start();
        auto sup = supervisor->access<rt::to::locality_leader>();
        send<payload::sample_payload_t>(sup->get_address());
    }
};

struct test_supervisor2_t : sample_supervisor_t {
    using sample_supervisor_t::sample_supervisor_t;

    r::actor_ptr_t child;

    void on_start() noexcept override {
        rt::supervisor_test_t::on_start();
        auto factory = [&](r::supervisor_t &sup, const r::address_ptr_t &spawner) -> r::actor_ptr_t {
            child = sup.create_actor<sample_actor_t>().timeout(rt::default_timeout).spawner_address(spawner).finish();
            return child;
        };
        spawn(factory).max_attempts(2).restart_policy(r::restart_policy_t::always).spawn();
        auto root = supervisor->access<rt::to::locality_leader>();
        send<payload::sample_payload_t>(root->get_address());
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
        sup->spawn(factory).restart_policy(r::restart_policy_t::never).restart_period(r::pt::seconds{1}).spawn();
        sup->do_process();
        CHECK(invocations == 1);
    }

    SECTION("restart_policy: ask_actor") {
        auto factory = [&](r::supervisor_t &, const r::address_ptr_t &) -> r::actor_ptr_t {
            ++invocations;
            throw std::runtime_error("does-not-matter");
        };
        sup->spawn(factory).restart_policy(r::restart_policy_t::ask_actor).spawn();
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

    SECTION("restart_policy: ask_actor") {
        sup->spawn(factory).restart_policy(r::restart_policy_t::ask_actor).spawn();
        sup->do_process();
        CHECK(invocations == 1);
        CHECK(samples == 1);

        auto *actor = static_cast<sample_actor_t *>(act.get());
        actor->restart_flag = true;
        act->do_shutdown(ee);
        act.reset();
        sup->trigger_timers_and_process();

        CHECK(act);
        CHECK(invocations == 2);
        CHECK(samples == 2);

        actor = static_cast<sample_actor_t *>(act.get());
        actor->restart_flag = false;
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

TEST_CASE("trees of supervisorts", "[spawner]") {
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

    SECTION("just with inner sup") {
        r::actor_ptr_t act;
        auto factory = [&](r::supervisor_t &, const r::address_ptr_t &spawner) -> r::actor_ptr_t {
            act =
                sup->create_actor<test_supervisor1_t>().timeout(rt::default_timeout).spawner_address(spawner).finish();
            return act;
        };

        sup->spawn(factory).restart_policy(r::restart_policy_t::always).max_attempts(2).spawn();
        sup->do_process();
        CHECK(samples == 1);

        act->do_shutdown();

        sup->trigger_timers_and_process();
        CHECK(samples == 2);

        sup->do_shutdown();
        sup->do_process();
        CHECK(samples == 2);
    }

    SECTION("inner sup with a child") {
        r::actor_ptr_t act;
        auto factory = [&](r::supervisor_t &, const r::address_ptr_t &spawner) -> r::actor_ptr_t {
            act =
                sup->create_actor<test_supervisor2_t>().timeout(rt::default_timeout).spawner_address(spawner).finish();
            return act;
        };

        sup->spawn(factory).restart_policy(r::restart_policy_t::always).max_attempts(2).spawn();
        sup->do_process();
        CHECK(samples == 2);

        static_cast<test_supervisor2_t *>(act.get())->child->do_shutdown();
        static_cast<test_supervisor2_t *>(act.get())->trigger_timers_and_process();
        CHECK(samples == 3);

        static_cast<test_supervisor2_t *>(act.get())->child->do_shutdown();
        static_cast<test_supervisor2_t *>(act.get())->trigger_timers_and_process();
        CHECK(samples == 3);

        act->do_shutdown();
        sup->trigger_timers_and_process();
        CHECK(samples == 5);

        static_cast<test_supervisor2_t *>(act.get())->child->do_shutdown();
        static_cast<test_supervisor2_t *>(act.get())->trigger_timers_and_process();
        CHECK(samples == 6);

        static_cast<test_supervisor2_t *>(act.get())->child->do_shutdown();
        static_cast<test_supervisor2_t *>(act.get())->trigger_timers_and_process();
        CHECK(samples == 6);

        act->do_shutdown();
        sup->trigger_timers_and_process();
        CHECK(samples == 6);

        sup->do_shutdown();
        sup->do_process();
        CHECK(samples == 6);
    }

    CHECK(sup->get_state() == r::state_t::SHUT_DOWN);
}

TEST_CASE("factory throws on supervisor initialization", "[spawner]") {
    r::system_context_t system_context;
    auto sup = system_context.create_supervisor<sample_supervisor_t>()
                   .timeout(rt::default_timeout)
                   .policy(r::supervisor_policy_t::shutdown_self)
                   .finish();
    auto factory = [&](r::supervisor_t &, const r::address_ptr_t &) -> r::actor_ptr_t { throw "does-not-matter"; };
    sup->spawn(factory).spawn();
    static_cast<r::actor_base_t *>(sup.get())->access<rt::to::resources>()->acquire();
    sup->do_process();
    static_cast<r::actor_base_t *>(sup.get())->access<rt::to::resources>()->release();
    sup->do_process();
    CHECK(sup->get_state() == r::state_t::SHUT_DOWN);
}

TEST_CASE("escalate failure", "[spawner]") {
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

    SECTION("restart_policy: always") {
        sup->spawn(factory).restart_policy(r::restart_policy_t::always).max_attempts(2).escalate_failure().spawn();
        sup->do_process();
        CHECK(invocations == 1);
        CHECK(samples == 1);

        act->do_shutdown(ee);
        act.reset();
        sup->trigger_timers_and_process();

        CHECK(act);
        CHECK(invocations == 2);
        CHECK(samples == 2);

        act->do_shutdown(ee);
        act.reset();
        sup->trigger_timers_and_process();
        CHECK(invocations == 2);
        CHECK(samples == 2);
    }

    SECTION("restart_policy: normal_only") {
        sup->spawn(factory).restart_policy(r::restart_policy_t::normal_only).max_attempts(2).escalate_failure().spawn();
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
        CHECK(invocations == 2);
        CHECK(samples == 2);
    }

    SECTION("restart_policy: fail_only + max_fails") {
        sup->spawn(factory).restart_policy(r::restart_policy_t::fail_only).max_attempts(2).escalate_failure().spawn();
        sup->do_process();
        CHECK(invocations == 1);
        CHECK(samples == 1);

        act->do_shutdown(ee);
        act.reset();
        sup->trigger_timers_and_process();

        CHECK(act);
        CHECK(invocations == 2);
        CHECK(samples == 2);

        act->do_shutdown(ee);
        act.reset();
        sup->trigger_timers_and_process();
        CHECK(invocations == 2);
        CHECK(samples == 2);
    }

    auto reason = sup->get_shutdown_reason();
    REQUIRE(reason);
    CHECK(reason->root() == ee);

    CHECK(sup->get_state() == r::state_t::SHUT_DOWN);
}
