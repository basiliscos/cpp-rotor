//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "supervisor_test.h"

namespace r = rotor;
namespace rt = r::test;

struct responce_sample_t {
    int value;
    responce_sample_t(int value_) : value{value_} {}
    responce_sample_t(const responce_sample_t &) = delete;
    responce_sample_t(responce_sample_t &&) = delete;
};

struct request_sample_t : r::arc_base_t<request_sample_t> {
    using responce_t = responce_sample_t;

    int value;
    request_sample_t(int value_) : value{value_} {}
    request_sample_t(const request_sample_t &) = delete;
    request_sample_t(request_sample_t &&) = delete;
};

using traits_t = r::request_traits_t<request_sample_t>;

struct good_actor_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;
    int req_val = 0;
    int res_val = 0;
    r::error_code_t ec = r::error_code_t::supervisor_defined;

    void on_initialize(r::message_t<r::payload::initialize_actor_t> &msg) noexcept override {
        r::actor_base_t::on_initialize(msg);
        subscribe(&good_actor_t::on_request);
        subscribe(&good_actor_t::on_responce);
    }

    void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        r::actor_base_t::on_start(msg);
        request<request_sample_t>(address, 4).timeout(r::pt::seconds(1));
    }

    void on_request(traits_t::request_message_t &msg) noexcept { reply_to(msg, 5); }

    void on_responce(traits_t::responce_message_t &msg) noexcept {
        req_val += msg.payload.req->payload.value;
        res_val += msg.payload.res->value;
        ec = msg.payload.ec;
    }
};

struct bad_actor_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;
    int req_val = 0;
    int res_val = 0;
    r::error_code_t ec = r::error_code_t::success;
    r::intrusive_ptr_t<traits_t::request_message_t> req_msg;

    void on_initialize(r::message_t<r::payload::initialize_actor_t> &msg) noexcept override {
        r::actor_base_t::on_initialize(msg);
        subscribe(&bad_actor_t::on_request);
        subscribe(&bad_actor_t::on_responce);
    }

    void on_shutdown(r::message_t<r::payload::shutdown_request_t> &msg) noexcept override  {
        r::actor_base_t::on_shutdown(msg);
        req_msg.reset();
    }

    void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        r::actor_base_t::on_start(msg);
        request<request_sample_t>(address, 4).timeout(r::pt::seconds(1));
    }

    void on_request(traits_t::request_message_t &msg) noexcept {
        req_msg.reset(&msg);
    }

    void on_responce(traits_t::responce_message_t &msg) noexcept {
        req_val += msg.payload.req->payload.value;
        ec = msg.payload.ec;
        if (msg.payload.res) {
            res_val = 9;
        }
    }
};

struct good_supervisor_t : rt::supervisor_test_t {
    int req_val = 0;
    int res_val = 0;
    r::error_code_t ec = r::error_code_t::supervisor_defined;

    using rt::supervisor_test_t::supervisor_test_t;

    void on_initialize(r::message_t<r::payload::initialize_actor_t> &msg) noexcept override {
        rt::supervisor_test_t::actor_base_t::on_initialize(msg);
        subscribe(&good_supervisor_t::on_request);
        subscribe(&good_supervisor_t::on_responce);
    }

    void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        rt::supervisor_test_t::on_start(msg);
        request<request_sample_t>(this->address, 4).timeout(r::pt::seconds(1));
    }

    void on_request(traits_t::request_message_t &msg) noexcept { reply_to(msg, 5); }

    void on_responce(traits_t::responce_message_t &msg) noexcept {
        req_val += msg.payload.req->payload.value;
        res_val += msg.payload.res->value;
        ec = msg.payload.ec;
    }
};

TEST_CASE("request-responce successfull delivery", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>(nullptr, r::pt::milliseconds{500}, nullptr);
    auto actor = sup->create_actor<good_actor_t>();
    sup->do_process();

    REQUIRE(sup->active_timers.size() == 0);
    REQUIRE(actor->req_val == 4);
    REQUIRE(actor->res_val == 5);
    REQUIRE(actor->ec == r::error_code_t::success);

    sup->do_shutdown();
    sup->do_process();

    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().size() == 0);
    REQUIRE(sup->get_children().size() == 0);
    REQUIRE(sup->get_requests().size() == 0);
    REQUIRE(sup->active_timers.size() == 0);
}

TEST_CASE("request-responce timeout", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>(nullptr, r::pt::milliseconds{500}, nullptr);
    auto actor = sup->create_actor<bad_actor_t>();
    sup->do_process();

    REQUIRE(actor->req_val == 0);
    REQUIRE(actor->res_val == 0);
    REQUIRE(sup->active_timers.size() == 1);
    REQUIRE(actor->ec == r::error_code_t::success);
    auto timer_id = *(sup->active_timers.begin());

    sup->on_timer_trigger(timer_id);
    sup->do_process();
    REQUIRE(actor->req_msg);
    REQUIRE(actor->req_val == 4);
    REQUIRE(actor->res_val == 0);
    REQUIRE(actor->ec == r::error_code_t::request_timeout);

    sup->active_timers.clear();

    actor->reply_to(*actor->req_msg, 1);
    sup->do_process();
    // nothing should be changed, i.e. reply should just be dropped
    REQUIRE(actor->req_val == 4);
    REQUIRE(actor->res_val == 0);
    REQUIRE(actor->ec == r::error_code_t::request_timeout);

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().size() == 0);
    REQUIRE(sup->active_timers.size() == 0);
}

TEST_CASE("request-responce successfull delivery (supervisor)", "[supervisor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<good_supervisor_t>(nullptr, r::pt::milliseconds{500}, nullptr);
    sup->do_process();

    REQUIRE(sup->active_timers.size() == 0);
    REQUIRE(sup->req_val == 4);
    REQUIRE(sup->res_val == 5);
    REQUIRE(sup->ec == r::error_code_t::success);

    sup->do_shutdown();
    sup->do_process();

    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().size() == 0);
    REQUIRE(sup->get_children().size() == 0);
    REQUIRE(sup->get_requests().size() == 0);
    REQUIRE(sup->active_timers.size() == 0);
}
