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
namespace rt = r::test;
namespace pt = boost::posix_time;

struct sample_res_t {};
struct sample_req_t {
    using response_t = sample_res_t;
};

using traits_t = r::request_traits_t<sample_req_t>;

struct bad_actor_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;
    std::error_code ec;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        r::actor_base_t::configure(plugin);
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&bad_actor_t::on_response); });
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        request<traits_t::request::type>(address).send(r::pt::milliseconds(1));
    }

    void on_response(traits_t::response::message_t &msg) noexcept {
        ec = msg.payload.ec;
        // alternative for supervisor.do_shutdown() for better coverage
        auto sup_addr = static_cast<r::actor_base_t *>(supervisor)->get_address();
        auto shutdown_trigger = r::make_message<r::payload::shutdown_trigger_t>(sup_addr, sup_addr);
        supervisor->enqueue(shutdown_trigger);
        auto loop = wxEventLoopBase::GetActive();
        loop->ScheduleExit();
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

    auto actor = sup->create_actor<bad_actor_t>().timeout(timeout).finish();

    sup->start();
    loop->Run();

    REQUIRE(actor->ec == r::error_code_t::request_timeout);

    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    CHECK(rt::empty(sup->get_subscription()));

    actor.reset();
    sup.reset();

    delete app;
}
