//
// Copyright (c) 2019-2024 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "access.h"
#include "rotor.hpp"
#include "supervisor_test.h"

namespace r = rotor;
namespace rt = r::test;

namespace payload {
struct sample_payload_t {};
} // namespace payload

namespace message {
using sample_message_t = r::message_t<payload::sample_payload_t>;
}

TEST_CASE("delivery to unknown addr", "[message]") {
    struct actor_t : public r::actor_base_t {
        using parent_t = r::actor_base_t;
        using parent_t::parent_t;

        void configure(r::plugin::plugin_base_t &plugin) noexcept override {
            r::actor_base_t::configure(plugin);
            plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&actor_t::on_destroy); });
        }

        void on_destroy(message::sample_message_t &) noexcept { ++samples_received; }

        int samples_received = 0;
    };

    r::system_context_t system_context;
    rt::supervisor_test_ptr_t sup;

    sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<actor_t>().timeout(rt::default_timeout).finish();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(act->samples_received == 0);

    auto fake_addr = sup->make_address();
    auto msg = r::make_routed_message<payload::sample_payload_t>(fake_addr, act->get_address());
    sup->put(std::move(msg));

    sup->do_process();
    REQUIRE(act->samples_received == 1);
}

TEST_CASE("delivery to local address, then route to destroy address", "[message]") {
    struct actor_t : public r::actor_base_t {
        using parent_t = r::actor_base_t;
        using parent_t::parent_t;

        void configure(r::plugin::plugin_base_t &plugin) noexcept override {
            r::actor_base_t::configure(plugin);
            plugin.with_casted<r::plugin::address_maker_plugin_t>([&](auto &p) { destroy_addr = p.create_address(); });
            plugin.with_casted<r::plugin::starter_plugin_t>([this](auto &p) {
                p.subscribe_actor(&actor_t::on_payoad);
                p.subscribe_actor(&actor_t::on_destroy, destroy_addr);
            });
        }

        void on_payoad(message::sample_message_t &) noexcept { samples_received += 2; }

        void on_destroy(message::sample_message_t &) noexcept { samples_received *= 10; }

        int samples_received = 0;
        r::address_ptr_t destroy_addr;
    };

    r::system_context_t system_context;
    rt::supervisor_test_ptr_t sup;

    sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<actor_t>().timeout(rt::default_timeout).finish();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(act->samples_received == 0);

    auto msg = r::make_routed_message<payload::sample_payload_t>(act->get_address(), act->destroy_addr);
    sup->put(std::move(msg));

    sup->do_process();
    REQUIRE(act->samples_received == 20);
}

TEST_CASE("delivery to external address, then route to destroy address", "[message]") {
    static int samples_received = 0;
    struct target_t : public r::actor_base_t {
        using parent_t = r::actor_base_t;
        using parent_t::parent_t;

        void configure(r::plugin::plugin_base_t &plugin) noexcept override {
            r::actor_base_t::configure(plugin);
            plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&target_t::on_payoad); });
        }

        void on_payoad(message::sample_message_t &) noexcept { samples_received += 2; }
    };

    struct sink_t : public r::actor_base_t {
        using parent_t = r::actor_base_t;
        using parent_t::parent_t;

        void configure(r::plugin::plugin_base_t &plugin) noexcept override {
            r::actor_base_t::configure(plugin);
            plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&sink_t::on_destroy); });
        }

        void on_destroy(message::sample_message_t &) noexcept { samples_received *= 10; }
    };

    r::system_context_t system_context;
    const char locality1[] = "l1";
    const char locality2[] = "l2";
    auto sup1 = system_context.create_supervisor<rt::supervisor_test_t>()
                    .locality(locality1)
                    .timeout(rt::default_timeout)
                    .finish();
    auto sup2 = sup1->create_actor<rt::supervisor_test_t>().locality(locality2).timeout(rt::default_timeout).finish();
    auto sink = sup1->create_actor<sink_t>().timeout(rt::default_timeout).finish();
    auto target = sup2->create_actor<target_t>().timeout(rt::default_timeout).finish();

    while (!sup1->get_leader_queue().empty() || !sup2->get_leader_queue().empty()) {
        sup1->do_process();
        sup2->do_process();
    }

    REQUIRE(sup1->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(sup2->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(samples_received == 0);

    auto msg = r::make_routed_message<payload::sample_payload_t>(target->get_address(), sink->get_address());
    sup1->put(std::move(msg));

    while (!sup1->get_leader_queue().empty() || !sup2->get_leader_queue().empty()) {
        sup1->do_process();
        sup2->do_process();
    }

    REQUIRE(samples_received == 20);
}

TEST_CASE("delivery to external subscriber, then route to destroy address", "[message]") {
    static int samples_received = 0;
    struct target_t : public r::actor_base_t {
        using parent_t = r::actor_base_t;
        using parent_t::parent_t;

        void configure(r::plugin::plugin_base_t &plugin) noexcept override {
            r::actor_base_t::configure(plugin);
            plugin.with_casted<r::plugin::starter_plugin_t>(
                [this](auto &p) { p.subscribe_actor(&target_t::on_payoad, sink_addr); });
        }

        void on_payoad(message::sample_message_t &) noexcept { samples_received += 2; }
        r::address_ptr_t sink_addr;
    };

    struct sink_t : public r::actor_base_t {
        using parent_t = r::actor_base_t;
        using parent_t::parent_t;

        void configure(r::plugin::plugin_base_t &plugin) noexcept override {
            r::actor_base_t::configure(plugin);
            plugin.with_casted<r::plugin::address_maker_plugin_t>([&](auto &p) { destroy_addr = p.create_address(); });
            plugin.with_casted<r::plugin::starter_plugin_t>(
                [this](auto &p) { p.subscribe_actor(&sink_t::on_destroy, destroy_addr); });
        }

        void on_destroy(message::sample_message_t &) noexcept { samples_received *= 10; }
        r::address_ptr_t destroy_addr;
    };

    r::system_context_t system_context;
    const char locality1[] = "l1";
    const char locality2[] = "l2";
    auto sup1 = system_context.create_supervisor<rt::supervisor_test_t>()
                    .locality(locality1)
                    .timeout(rt::default_timeout)
                    .finish();
    auto sup2 = sup1->create_actor<rt::supervisor_test_t>().locality(locality2).timeout(rt::default_timeout).finish();
    auto sink = sup1->create_actor<sink_t>().timeout(rt::default_timeout).finish();
    auto target = sup2->create_actor<target_t>().timeout(rt::default_timeout).finish();
    target->sink_addr = sink->get_address();

    while (!sup1->get_leader_queue().empty() || !sup2->get_leader_queue().empty()) {
        sup1->do_process();
        sup2->do_process();
    }

    REQUIRE(sup1->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(sup2->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(samples_received == 0);

    auto msg = r::make_routed_message<payload::sample_payload_t>(sink->get_address(), sink->destroy_addr);
    sup1->put(std::move(msg));

    while (!sup1->get_leader_queue().empty() || !sup2->get_leader_queue().empty()) {
        sup1->do_process();
        sup2->do_process();
    }

    REQUIRE(samples_received == 20);
}
