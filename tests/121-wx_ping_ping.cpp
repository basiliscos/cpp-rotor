//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "rotor/wx.hpp"
#include "supervisor_wx_test.h"
#include <wx/evtloop.h>
#include <wx/apptrait.h>
#include <iostream>

IMPLEMENT_APP_NO_MAIN(rotor::test::RotorApp)

namespace r = rotor;
namespace rx = rotor::wx;
namespace rt = rotor::test;

struct ping_t {};
struct pong_t {};

static std::uint32_t destroyed = 0;

struct pinger_t : public r::actor_base_t {
    std::uint32_t ping_sent;
    std::uint32_t pong_received;
    rotor::address_ptr_t ponger_addr;

    pinger_t(rotor::supervisor_t &sup) : r::actor_base_t{sup} { ping_sent = pong_received = 0; }
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
        auto loop = wxEventLoopBase::GetActive();
        loop->ScheduleExit();
    }
};

struct ponger_t : public r::actor_base_t {
    std::uint32_t pong_sent;
    std::uint32_t ping_received;
    rotor::address_ptr_t pinger_addr;

    ponger_t(rotor::supervisor_t &sup) : rotor::actor_base_t{sup} { pong_sent = ping_received = 0; }
    ~ponger_t() { destroyed += 3; }

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

TEST_CASE("ping/pong ", "[supervisor][wx]") {
    using app_t = rotor::test::RotorApp;
    auto app = new app_t();

    auto timeout = r::pt::milliseconds{10};
    app->CallOnInit();
    wxEventLoopBase *loop = app->GetTraits()->CreateEventLoop();
    wxEventLoopBase::SetActive(loop);
    rx::system_context_ptr_t system_context{new rx::system_context_wx_t(app)};
    wxEvtHandler handler;
    rx::supervisor_config_wx_t conf{timeout, &handler};
    auto sup = system_context->create_supervisor<rt::supervisor_wx_test_t>(conf);
    sup->start();

    auto pinger = sup->create_actor<pinger_t>(timeout);
    auto ponger = sup->create_actor<ponger_t>(timeout);
    pinger->set_ponger_addr(ponger->get_address());
    ponger->set_pinger_addr(pinger->get_address());

    sup->start();
    loop->Run();

    REQUIRE(pinger->ping_sent == 1);
    REQUIRE(pinger->pong_received == 1);
    REQUIRE(ponger->pong_sent == 1);
    REQUIRE(ponger->ping_received == 1);

    pinger.reset();
    ponger.reset();
    REQUIRE(destroyed == 4);

    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().size() == 0);

    delete app;
}
