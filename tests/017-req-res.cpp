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
};

struct request_sample_t {
    using responce_t = responce_sample_t;

    int value;
};

struct res2_t : r::arc_base_t<res2_t> {
    int value;
    res2_t(int value_) : value{value_} {}
    virtual ~res2_t() {}
};

struct req2_t {
    using responce_t = r::intrusive_ptr_t<res2_t>;

    int value;
};

using traits_t = r::request_traits_t<request_sample_t>;

struct good_actor_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;
    int req_val = 0;
    int res_val = 0;
    std::error_code ec;

    void on_initialize(r::message::init_request_t &msg) noexcept override {
        r::actor_base_t::on_initialize(msg);
        subscribe(&good_actor_t::on_request);
        subscribe(&good_actor_t::on_responce);
    }

    void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        r::actor_base_t::on_start(msg);
        request<request_sample_t>(address, 4).timeout(r::pt::seconds(1));
    }

    void on_request(traits_t::request::message_t &msg) noexcept { reply_to(msg, 5); }

    void on_responce(traits_t::responce::message_t &msg) noexcept {
        req_val += msg.payload.req->payload.request_payload.value;
        res_val += msg.payload.res.value;
        ec = msg.payload.ec;
    }
};

struct bad_actor_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;
    int req_val = 0;
    int res_val = 0;
    std::error_code ec;
    r::intrusive_ptr_t<traits_t::request::message_t> req_msg;

    void on_initialize(r::message::init_request_t &msg) noexcept override {
        r::actor_base_t::on_initialize(msg);
        subscribe(&bad_actor_t::on_request);
        subscribe(&bad_actor_t::on_responce);
    }

    void on_shutdown(r::message::shutdown_request_t &msg) noexcept override {
        r::actor_base_t::on_shutdown(msg);
        req_msg.reset();
    }

    void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        r::actor_base_t::on_start(msg);
        request<request_sample_t>(address, 4).timeout(r::pt::seconds(1));
    }

    void on_request(traits_t::request::message_t &msg) noexcept { req_msg.reset(&msg); }

    void on_responce(traits_t::responce::message_t &msg) noexcept {
        req_val += msg.payload.req->payload.request_payload.value;
        ec = msg.payload.ec;
        if (!ec) {
            res_val += 9;
        }
    }
};

struct bad_actor2_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;
    int req_val = 0;
    int res_val = 0;
    std::error_code ec;

    void init_start() noexcept override {
        subscribe(&bad_actor2_t::on_request);
        subscribe(&bad_actor2_t::on_responce);
        r::actor_base_t::init_start();
    }

    void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        r::actor_base_t::on_start(msg);
        request<request_sample_t>(address, 4).timeout(r::pt::seconds(1));
    }

    void on_request(traits_t::request::message_t &msg) noexcept {
        reply_with_error(msg, r::make_error_code(r::error_code_t::request_timeout));
    }

    void on_responce(traits_t::responce::message_t &msg) noexcept {
        req_val += msg.payload.req->payload.request_payload.value;
        ec = msg.payload.ec;
        if (!ec) {
            res_val += 9;
        }
    }
};

struct good_supervisor_t : rt::supervisor_test_t {
    int req_val = 0;
    int res_val = 0;
    std::error_code ec;

    using rt::supervisor_test_t::supervisor_test_t;

    void on_initialize(r::message::init_request_t &msg) noexcept override {
        rt::supervisor_test_t::actor_base_t::on_initialize(msg);
        subscribe(&good_supervisor_t::on_request);
        subscribe(&good_supervisor_t::on_responce);
    }

    void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        rt::supervisor_test_t::on_start(msg);
        request<request_sample_t>(this->address, 4).timeout(r::pt::seconds(1));
    }

    void on_request(traits_t::request::message_t &msg) noexcept { reply_to(msg, 5); }

    void on_responce(traits_t::responce::message_t &msg) noexcept {
        req_val += msg.payload.req->payload.request_payload.value;
        res_val += msg.payload.res.value;
        ec = msg.payload.ec;
    }
};

struct good_actor2_t : public r::actor_base_t {
    using traits2_t = r::request_traits_t<req2_t>;

    using r::actor_base_t::actor_base_t;

    int req_val = 0;
    int res_val = 0;
    r::address_ptr_t reply_addr;
    std::error_code ec;

    void on_initialize(r::message::init_request_t &msg) noexcept override {
        reply_addr = supervisor.create_address();
        subscribe(&good_actor2_t::on_request);
        subscribe(&good_actor2_t::on_responce, reply_addr);
        r::actor_base_t::on_initialize(msg);
    }

    void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        r::actor_base_t::on_start(msg);
        request_via<req2_t>(address, reply_addr, 4).timeout(r::pt::seconds(1));
    }

    void on_request(traits2_t::request::message_t &msg) noexcept { reply_to(msg, 5); }

    void on_responce(traits2_t::responce::message_t &msg) noexcept {
        req_val += msg.payload.req->payload.request_payload.value;
        res_val += msg.payload.res->value;
        ec = msg.payload.ec;
    }
};

struct good_actor3_t : public r::actor_base_t {
    using traits2_t = r::request_traits_t<req2_t>;

    using r::actor_base_t::actor_base_t;

    int req_left = 1;
    int req_val = 0;
    int res_val = 0;
    std::error_code ec;

    void on_initialize(r::message::init_request_t &msg) noexcept override {
        r::actor_base_t::on_initialize(msg);
        subscribe(&good_actor3_t::on_request);
        subscribe(&good_actor3_t::on_responce);
    }

    void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        r::actor_base_t::on_start(msg);
        request<req2_t>(address, 4).timeout(r::pt::seconds(1));
    }

    void on_request(traits2_t::request::message_t &msg) noexcept { reply_to(msg, 5); }

    void on_responce(traits2_t::responce::message_t &msg) noexcept {
        req_val += msg.payload.req->payload.request_payload.value;
        res_val += msg.payload.res->value;
        ec = msg.payload.ec;
        if (req_left) {
            --req_left;
            request<req2_t>(address, 4).timeout(r::pt::seconds(1));
        }
    }
};

TEST_CASE("request-responce successfull delivery", "[actor]") {
    r::system_context_t system_context;

    auto timeout = r::pt::milliseconds{1};
    rt::supervisor_config_test_t config(timeout, nullptr);
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>(nullptr, config);
    auto actor = sup->create_actor<good_actor_t>(timeout);
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

TEST_CASE("request-responce successfull delivery indentical message to 2 actors", "[actor]") {
    r::system_context_t system_context;

    auto timeout = r::pt::milliseconds{1};
    rt::supervisor_config_test_t config(timeout, nullptr);
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>(nullptr, config);
    auto actor1 = sup->create_actor<good_actor_t>(timeout);
    auto actor2 = sup->create_actor<good_actor_t>(timeout);
    sup->do_process();

    REQUIRE(sup->active_timers.size() == 0);
    REQUIRE(actor1->req_val == 4);
    REQUIRE(actor1->res_val == 5);
    REQUIRE(actor1->ec == r::error_code_t::success);
    REQUIRE(actor2->req_val == 4);
    REQUIRE(actor2->res_val == 5);
    REQUIRE(actor2->ec == r::error_code_t::success);

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

    auto timeout = r::pt::milliseconds{1};
    rt::supervisor_config_test_t config(timeout, nullptr);
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>(nullptr, config);
    auto actor = sup->create_actor<bad_actor_t>(timeout);
    sup->do_process();

    REQUIRE(actor->req_val == 0);
    REQUIRE(actor->res_val == 0);
    REQUIRE(sup->active_timers.size() == 1);
    REQUIRE(!actor->ec);
    auto timer_id = *(sup->active_timers.begin());

    sup->on_timer_trigger(timer_id);
    sup->do_process();
    REQUIRE(actor->req_msg);
    REQUIRE(actor->req_val == 4);
    REQUIRE(actor->res_val == 0);
    REQUIRE(actor->ec);
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

TEST_CASE("responce with custom error", "[actor]") {
    r::system_context_t system_context;

    auto timeout = r::pt::milliseconds{1};
    rt::supervisor_config_test_t config(timeout, nullptr);
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>(nullptr, config);
    auto actor = sup->create_actor<bad_actor2_t>(timeout);
    sup->do_process();

    REQUIRE(actor->req_val == 4);
    REQUIRE(actor->res_val == 0);
    REQUIRE(actor->ec);
    REQUIRE(actor->ec == r::error_code_t::request_timeout);
    REQUIRE(sup->active_timers.size() == 0);

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().size() == 0);
}

TEST_CASE("request-responce successfull delivery (supervisor)", "[supervisor]") {
    r::system_context_t system_context;

    auto timeout = r::pt::milliseconds{1};
    rt::supervisor_config_test_t config(timeout, nullptr);
    auto sup = system_context.create_supervisor<good_supervisor_t>(nullptr, config);
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

TEST_CASE("request-responce successfull delivery, ref-counted responce", "[actor]") {
    r::system_context_t system_context;

    auto timeout = r::pt::milliseconds{1};
    rt::supervisor_config_test_t config(timeout, nullptr);
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>(nullptr, config);
    auto actor = sup->create_actor<good_actor2_t>(timeout);
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

TEST_CASE("request-responce successfull delivery, twice", "[actor]") {
    r::system_context_t system_context;

    auto timeout = r::pt::milliseconds{1};
    rt::supervisor_config_test_t config(timeout, nullptr);
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>(nullptr, config);
    auto actor = sup->create_actor<good_actor3_t>(timeout);
    sup->do_process();

    REQUIRE(sup->active_timers.size() == 0);
    REQUIRE(actor->req_val == 4 * 2);
    REQUIRE(actor->res_val == 5 * 2);
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
