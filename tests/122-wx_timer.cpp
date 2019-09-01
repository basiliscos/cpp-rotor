//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "rotor/asio.hpp"
#include "rotor/wx.hpp"
#include "supervisor_wx_test.h"
#include <wx/evtloop.h>
#include <wx/apptrait.h>

IMPLEMENT_APP_NO_MAIN(rotor::test::RotorApp)

namespace r = rotor;
namespace rx = rotor::wx;
namespace rt = r::test;
namespace asio = boost::asio;
namespace pt = boost::posix_time;

struct sample_res_t {};
struct sample_req_t {
    using responce_t = sample_res_t;
};

using traits_t = r::request_traits_t<sample_req_t>;

struct bad_actor_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;
    std::error_code ec;

    void on_initialize(r::message::init_request_t &msg) noexcept override {
        r::actor_base_t::on_initialize(msg);
        subscribe(&bad_actor_t::on_responce);
    }

    void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        r::actor_base_t::on_start(msg);
        request<traits_t::request::type>(address).timeout(r::pt::milliseconds(1));
    }

    void on_responce(traits_t::responce::message_t &msg) noexcept {
        ec = msg.payload.ec;
        // alternative for supervisor.do_shutdown() for better coverage
        auto sup_addr = supervisor.get_address();
        auto shutdown_trigger = r::make_message<r::payload::shutdown_trigger_t>(sup_addr, sup_addr);
        supervisor.enqueue(shutdown_trigger);
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
    rx::supervisor_config_wx_t conf{timeout, &handler};
    auto sup = system_context->create_supervisor<rt::supervisor_wx_test_t>(conf);
    sup->start();

    auto actor = sup->create_actor<bad_actor_t>(timeout);

    sup->start();
    loop->Run();

    REQUIRE(actor->ec == r::error_code_t::request_timeout);

    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().size() == 0);

    actor.reset();
    sup.reset();

    delete app;
}
