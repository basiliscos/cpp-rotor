//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "rotor/ev.hpp"
#include "access.h"
#include <ev.h>

namespace r = rotor;
namespace re = rotor::ev;
namespace rt = r::test;

static std::uint32_t destroyed = 0;

struct supervisor_ev_test_t : public re::supervisor_ev_t {
    using re::supervisor_ev_t::supervisor_ev_t;

    ~supervisor_ev_test_t() {
        destroyed += 4;
        printf("~supervisor_ev_test_t\n");
    }

    auto get_leader_queue() {
        return static_cast<supervisor_t *>(this)->access<rt::to::locality_leader>()->access<rt::to::queue>();
    }
    auto &get_subscription() noexcept { return subscription_map; }
};

struct self_shutdowner_sup_t : public re::supervisor_ev_t {
    using re::supervisor_ev_t::supervisor_ev_t;

    void init_finish() noexcept override {
        re::supervisor_ev_t::init_finish();
        printf("triggering shutdown\n");
        parent->shutdown();
    }

    void shutdown_finish() noexcept override {
        re::supervisor_ev_t::shutdown_finish();
        printf("shutdown_finish\n");
    }
};

struct system_context_ev_test_t : public re::system_context_ev_t {
    std::error_code code;
    void on_error(const std::error_code &ec) noexcept override {
        code = ec;
        auto loop = static_cast<re::supervisor_ev_t *>(get_supervisor().get())->get_loop();
        ev_break(loop);
    }
};

struct ping_t {};
struct pong_t {};

struct pinger_t : public r::actor_base_t {
    std::uint32_t ping_sent = 0;
    std::uint32_t pong_received = 0;
    rotor::address_ptr_t ponger_addr;

    using r::actor_base_t::actor_base_t;
    ~pinger_t() { destroyed += 1; }

    void set_ponger_addr(const rotor::address_ptr_t &addr) { ponger_addr = addr; }

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        r::actor_base_t::configure(plugin);
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&pinger_t::on_pong); });
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        send<ping_t>(ponger_addr);
        ++ping_sent;
    }

    void on_pong(rotor::message_t<pong_t> &) noexcept {
        ++pong_received;
        supervisor->shutdown();
    }
};

struct ponger_t : public r::actor_base_t {
    std::uint32_t pong_sent = 0;
    std::uint32_t ping_received = 0;
    rotor::address_ptr_t pinger_addr;

    using r::actor_base_t::actor_base_t;
    ~ponger_t() { destroyed += 2; }

    void set_pinger_addr(const rotor::address_ptr_t &addr) { pinger_addr = addr; }

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&ponger_t::on_ping); });
    }

    void on_ping(rotor::message_t<ping_t> &) noexcept {
        ++ping_received;
        send<pong_t>(pinger_addr);
        ++pong_sent;
    }
};

struct bad_actor_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;

    void on_start() noexcept override { supervisor->shutdown(); }

    void shutdown_finish() noexcept override {}

    ~bad_actor_t() { printf("~bad_actor_t\n"); }
};

TEST_CASE("ping/pong", "[supervisor][ev]") {
    auto *loop = ev_loop_new(0);
    auto system_context = re::system_context_ptr_t{new re::system_context_ev_t()};
    auto timeout = r::pt::milliseconds{10};
    auto sup = system_context->create_supervisor<supervisor_ev_test_t>()
                   .loop(loop)
                   .loop_ownership(true)
                   .timeout(timeout)
                   .finish();

    auto pinger = sup->create_actor<pinger_t>().timeout(timeout).finish();
    auto ponger = sup->create_actor<ponger_t>().timeout(timeout).finish();
    pinger->set_ponger_addr(static_cast<r::actor_base_t *>(ponger.get())->get_address());
    ponger->set_pinger_addr(static_cast<r::actor_base_t *>(pinger.get())->get_address());

    sup->start();
    ev_run(loop);

    REQUIRE(pinger->ping_sent == 1);
    REQUIRE(pinger->pong_received == 1);
    REQUIRE(ponger->pong_sent == 1);
    REQUIRE(ponger->ping_received == 1);

    pinger.reset();
    ponger.reset();

    REQUIRE(static_cast<r::actor_base_t *>(sup.get())->access<rt::to::state>() == r::state_t::SHUT_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    CHECK(rt::empty(sup->get_subscription()));

    sup.reset();
    system_context.reset();

    REQUIRE(destroyed == 1 + 2 + 4);
}

TEST_CASE("no shutdown confirmation", "[supervisor][ev]") {
    auto destroyed_start = destroyed;
    auto *loop = ev_loop_new(0);
    auto system_context = r::intrusive_ptr_t<system_context_ev_test_t>{new system_context_ev_test_t()};
    auto timeout = r::pt::milliseconds{10};
    auto sup = system_context->create_supervisor<supervisor_ev_test_t>()
                   .loop(loop)
                   .loop_ownership(true)
                   .timeout(timeout)
                   .finish();

    sup->start();
    sup->create_actor<bad_actor_t>().timeout(timeout).finish();
    ev_run(loop);

    REQUIRE(system_context->code.value() == static_cast<int>(r::error_code_t::request_timeout));

    // act->force_cleanup();
    sup->shutdown();
    ev_run(loop);
    sup.reset();
    system_context.reset();
    auto destroyed_end = destroyed;
    CHECK(destroyed_end == destroyed_start + 4);
}

TEST_CASE("supervisors hierarchy", "[supervisor][ev]") {
    auto *loop = ev_loop_new(0);
    auto system_context = r::intrusive_ptr_t<system_context_ev_test_t>{new system_context_ev_test_t()};
    auto timeout = r::pt::milliseconds{10};
    auto sup = system_context->create_supervisor<supervisor_ev_test_t>()
                   .loop(loop)
                   .loop_ownership(true)
                   .timeout(timeout)
                   .finish();

    auto act = sup->create_actor<self_shutdowner_sup_t>().loop(loop).loop_ownership(false).timeout(timeout).finish();
    sup->start();
    ev_run(loop);
    CHECK(!system_context->code);

    CHECK(((r::actor_base_t *)act.get())->access<rt::to::state>() == r::state_t::SHUT_DOWN);
    CHECK(((r::actor_base_t *)sup.get())->access<rt::to::state>() == r::state_t::SHUT_DOWN);
}
