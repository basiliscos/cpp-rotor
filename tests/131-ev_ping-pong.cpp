//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "rotor/ev.hpp"
#include <ev.h>

namespace r = rotor;
namespace re = rotor::ev;

static std::uint32_t destroyed = 0;

struct supervisor_test_behavior_t : public r::supervisor_behavior_t {
    using r::supervisor_behavior_t::supervisor_behavior_t;

    void on_shutdown_fail(const r::address_ptr_t &address, const std::error_code &ec) noexcept override;
};

struct supervisor_ev_test_t : public re::supervisor_ev_t {
    using re::supervisor_ev_t::supervisor_ev_t;

    ~supervisor_ev_test_t() { destroyed += 4; }

    virtual r::actor_behavior_t *create_behavior() noexcept override { return new supervisor_test_behavior_t(*this); }

    r::state_t &get_state() noexcept { return state; }
    queue_t &get_leader_queue() { return get_leader().queue; }
    supervisor_ev_test_t &get_leader() { return *static_cast<supervisor_ev_test_t *>(locality_leader); }
    queue_t &get_inbound_queue() noexcept { return inbound; }
    subscription_points_t &get_points() noexcept { return points; }
    subscription_map_t &get_subscription() noexcept { return subscription_map; }
};

void supervisor_test_behavior_t::on_shutdown_fail(const r::address_ptr_t &address, const std::error_code &ec) noexcept {
    r::supervisor_behavior_t::on_shutdown_fail(address, ec);
    auto loop = static_cast<supervisor_ev_test_t &>(actor).get_loop();
    ev_break(loop);
}

struct system_context_ev_test_t : public re::system_context_ev_t {
    std::error_code code;
    void on_error(const std::error_code &ec) noexcept override { code = ec; }
};

struct ping_t {};
struct pong_t {};

struct pinger_t : public r::actor_base_t {
    std::uint32_t ping_sent;
    std::uint32_t pong_received;
    rotor::address_ptr_t ponger_addr;

    explicit pinger_t(rotor::supervisor_t &sup) : r::actor_base_t{sup} { ping_sent = pong_received = 0; }
    ~pinger_t() { destroyed += 1; }

    void set_ponger_addr(const rotor::address_ptr_t &addr) { ponger_addr = addr; }

    void init_start() noexcept override {
        subscribe(&pinger_t::on_pong);
        r::actor_base_t::init_start();
    }

    void on_start(rotor::message_t<rotor::payload::start_actor_t> &msg) noexcept override {
        r::actor_base_t::on_start(msg);
        send<ping_t>(ponger_addr);
        ++ping_sent;
    }

    void on_pong(rotor::message_t<pong_t> &) noexcept {
        ++pong_received;
        supervisor.shutdown();
    }
};

struct ponger_t : public r::actor_base_t {
    std::uint32_t pong_sent;
    std::uint32_t ping_received;
    rotor::address_ptr_t pinger_addr;

    explicit ponger_t(rotor::supervisor_t &sup) : rotor::actor_base_t{sup} { pong_sent = ping_received = 0; }
    ~ponger_t() { destroyed += 2; }

    void set_pinger_addr(const rotor::address_ptr_t &addr) { pinger_addr = addr; }

    void init_start() noexcept override {
        subscribe(&ponger_t::on_ping);
        r::actor_base_t::init_start();
    }

    void on_ping(rotor::message_t<ping_t> &) noexcept {
        ++ping_received;
        send<pong_t>(pinger_addr);
        ++pong_sent;
    }
};

struct bad_actor_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;
    bool allow_shutdown = false;

    virtual void on_start(r::message_t<r::payload::start_actor_t> &) noexcept override { supervisor.do_shutdown(); }

    void shutdown_start() noexcept override {
        if (allow_shutdown) {
            r::actor_base_t::shutdown_start();
        }
    }
};

TEST_CASE("ping/pong", "[supervisor][ev]") {
    auto *loop = ev_loop_new(0);
    auto system_context = re::system_context_ev_t::ptr_t{new re::system_context_ev_t()};
    auto timeout = r::pt::milliseconds{10};
    auto conf = re::supervisor_config_ev_t{timeout, loop, true};
    auto sup = system_context->create_supervisor<supervisor_ev_test_t>(conf);

    auto pinger = sup->create_actor<pinger_t>(timeout);
    auto ponger = sup->create_actor<ponger_t>(timeout);
    pinger->set_ponger_addr(ponger->get_address());
    ponger->set_pinger_addr(pinger->get_address());

    sup->start();
    ev_run(loop);

    REQUIRE(pinger->ping_sent == 1);
    REQUIRE(pinger->pong_received == 1);
    REQUIRE(ponger->pong_sent == 1);
    REQUIRE(ponger->ping_received == 1);

    pinger.reset();
    ponger.reset();

    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().size() == 0);

    sup.reset();
    system_context.reset();

    REQUIRE(destroyed == 1 + 2 + 4);
}

TEST_CASE("error : create root supervisor twice", "[supervisor][ev]") {
    auto *loop = ev_loop_new(0);
    auto system_context = r::intrusive_ptr_t<system_context_ev_test_t>{new system_context_ev_test_t()};
    auto timeout = r::pt::milliseconds{10};
    auto conf = re::supervisor_config_ev_t{timeout, loop, true};
    auto sup1 = system_context->create_supervisor<supervisor_ev_test_t>(conf);
    REQUIRE(system_context->code.value() == 0);

    auto sup2 = system_context->create_supervisor<supervisor_ev_test_t>(conf);
    REQUIRE(!sup2);
    REQUIRE(system_context->code.value() == static_cast<int>(r::error_code_t::supervisor_defined));

    sup1->shutdown();
    ev_run(loop);

    sup1.reset();
    system_context.reset();
}

TEST_CASE("no shutdown confirmation", "[supervisor][ev]") {
    auto *loop = ev_loop_new(0);
    auto system_context = r::intrusive_ptr_t<system_context_ev_test_t>{new system_context_ev_test_t()};
    auto timeout = r::pt::milliseconds{10};
    auto conf = re::supervisor_config_ev_t{timeout, loop, true};
    auto sup = system_context->create_supervisor<supervisor_ev_test_t>(conf);

    sup->start();
    auto actor = sup->create_actor<bad_actor_t>(timeout);
    ev_run(loop);

    REQUIRE(system_context->code.value() == static_cast<int>(r::error_code_t::request_timeout));

    actor->allow_shutdown = true;
    sup->shutdown();
    ev_run(loop);

    sup.reset();
    system_context.reset();
}
