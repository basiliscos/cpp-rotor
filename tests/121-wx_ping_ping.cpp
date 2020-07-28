//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "rotor/wx.hpp"
#include "supervisor_wx_test.h"
#include <wx/evtloop.h>
#include <wx/apptrait.h>
#include "access.h"

IMPLEMENT_APP_NO_MAIN(rotor::test::RotorApp)

namespace r = rotor;
namespace rx = rotor::wx;
namespace rt = rotor::test;

struct ping_t {};
struct pong_t {};

static std::uint32_t destroyed = 0;

struct pinger_t : public r::actor_base_t {
    std::uint32_t ping_sent = 0;
    std::uint32_t pong_received = 0;
    rotor::address_ptr_t ponger_addr;

    using r::actor_base_t::actor_base_t;
    ~pinger_t() { destroyed += 1; }

    void set_ponger_addr(const rotor::address_ptr_t &addr) { ponger_addr = addr; }

    void configure(r::plugin_base_t &plugin) noexcept override {
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
        auto loop = wxEventLoopBase::GetActive();
        loop->ScheduleExit();
    }
};

struct ponger_t : public r::actor_base_t {
    std::uint32_t pong_sent = 0;
    std::uint32_t ping_received = 0;
    rotor::address_ptr_t pinger_addr;

    using r::actor_base_t::actor_base_t;

    ~ponger_t() { destroyed += 3; }

    void set_pinger_addr(const rotor::address_ptr_t &addr) { pinger_addr = addr; }

    void configure(r::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&ponger_t::on_ping); });
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
    auto sup =
        system_context->create_supervisor<rt::supervisor_wx_test_t>().handler(&handler).timeout(timeout).finish();
    sup->start();

    auto pinger = sup->create_actor<pinger_t>().timeout(timeout).finish();
    auto ponger = sup->create_actor<ponger_t>().timeout(timeout).finish();
    pinger->set_ponger_addr(static_cast<r::actor_base_t *>(ponger.get())->get_address());
    ponger->set_pinger_addr(static_cast<r::actor_base_t *>(pinger.get())->get_address());

    sup->start();
    loop->Run();

    REQUIRE(pinger->ping_sent == 1);
    REQUIRE(pinger->pong_received == 1);
    REQUIRE(ponger->pong_sent == 1);
    REQUIRE(ponger->ping_received == 1);

    pinger.reset();
    ponger.reset();
    REQUIRE(destroyed == 4);

    REQUIRE(static_cast<r::actor_base_t *>(sup.get())->access<rt::to::state>() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    CHECK(rt::empty(sup->get_subscription()));

    delete app;
}
