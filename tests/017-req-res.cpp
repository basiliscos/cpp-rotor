//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "supervisor_test.h"

namespace r = rotor;
namespace rt = r::test;

struct response_sample_t {
    int value;
};

struct request_sample_t {
    using response_t = response_sample_t;

    int value;
};

struct res2_t : r::arc_base_t<res2_t> {
    int value;
    explicit res2_t(int value_) : value{value_} {}
    virtual ~res2_t() {}
};

struct req2_t {
    using response_t = r::intrusive_ptr_t<res2_t>;

    int value;
};

struct res3_t : r::arc_base_t<res3_t> {
    int value;
    explicit res3_t(int value_) : value{value_} {}
    res3_t(const res3_t &) = delete;
    res3_t(res3_t &&) = delete;
    virtual ~res3_t() {}
};

struct req3_t : r::arc_base_t<req3_t> {

    using response_t = r::intrusive_ptr_t<res3_t>;
    int value;

    explicit req3_t(int value_) : value{value_} {}
    req3_t(const req3_t &) = delete;
    req3_t(req3_t &&) = delete;

    virtual ~req3_t() {}
};

static_assert(std::is_base_of_v<r::arc_base_t<req3_t>, req3_t>, "zzz");

using traits_t = r::request_traits_t<request_sample_t>;

struct good_actor_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;
    int req_val = 0;
    int res_val = 0;
    std::error_code ec;

    void configure(r::plugin_t &plugin) noexcept override {
        plugin.with_casted<r::internal::starter_plugin_t>([](auto &p) {
            p.subscribe_actor(&good_actor_t::on_request);
            p.subscribe_actor(&good_actor_t::on_response);
        });
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        request<request_sample_t>(address, 4).send(r::pt::seconds(1));
    }

    void on_request(traits_t::request::message_t &msg) noexcept { reply_to(msg, 5); }

    void on_response(traits_t::response::message_t &msg) noexcept {
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

    void configure(r::plugin_t &plugin) noexcept override {
        plugin.with_casted<r::internal::starter_plugin_t>([](auto &p) {
            p.subscribe_actor(&bad_actor_t::on_request);
            p.subscribe_actor(&bad_actor_t::on_response);
        });
    }

    void shutdown_start() noexcept override {
        req_msg.reset();
        r::actor_base_t::shutdown_start();
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        request<request_sample_t>(address, 4).send(rt::default_timeout);
    }

    void on_request(traits_t::request::message_t &msg) noexcept { req_msg.reset(&msg); }

    void on_response(traits_t::response::message_t &msg) noexcept {
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

    void configure(r::plugin_t &plugin) noexcept override {
        plugin.with_casted<r::internal::starter_plugin_t>([](auto &p) {
            p.subscribe_actor(&bad_actor2_t::on_request);
            p.subscribe_actor(&bad_actor2_t::on_response);
        });
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        request<request_sample_t>(address, 4).send(rt::default_timeout);
    }

    void on_request(traits_t::request::message_t &msg) noexcept {
        reply_with_error(msg, r::make_error_code(r::error_code_t::request_timeout));
    }

    void on_response(traits_t::response::message_t &msg) noexcept {
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

    void configure(r::plugin_t &plugin) noexcept override {
        plugin.with_casted<r::internal::starter_plugin_t>([](auto &p) {
            p.subscribe_actor(&good_supervisor_t::on_request);
            p.subscribe_actor(&good_supervisor_t::on_response);
        });
    }

    void on_start() noexcept override {
        rt::supervisor_test_t::on_start();
        request<request_sample_t>(this->address, 4).send(rt::default_timeout);
    }

    void on_request(traits_t::request::message_t &msg) noexcept { reply_to(msg, 5); }

    void on_response(traits_t::response::message_t &msg) noexcept {
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

    void configure(r::plugin_t &plugin) noexcept override {
        plugin.with_casted<r::internal::starter_plugin_t>([this](auto &p) {
            reply_addr = create_address();
            p.subscribe_actor(&good_actor2_t::on_response, reply_addr);
            p.subscribe_actor(&good_actor2_t::on_request);
        });
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        request_via<req2_t>(address, reply_addr, 4).send(rt::default_timeout);
    }

    void on_request(traits2_t::request::message_t &msg) noexcept { reply_to(msg, 5); }

    void on_response(traits2_t::response::message_t &msg) noexcept {
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

    void configure(r::plugin_t &plugin) noexcept override {
        plugin.with_casted<r::internal::starter_plugin_t>([](auto &p) {
            p.subscribe_actor(&good_actor3_t::on_response);
            p.subscribe_actor(&good_actor3_t::on_request);
        });
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        request<req2_t>(address, 4).send(rt::default_timeout);
    }

    void on_request(traits2_t::request::message_t &msg) noexcept { reply_to(msg, 5); }

    void on_response(traits2_t::response::message_t &msg) noexcept {
        req_val += msg.payload.req->payload.request_payload.value;
        res_val += msg.payload.res->value;
        ec = msg.payload.ec;
        if (req_left) {
            --req_left;
            request<req2_t>(address, 4).send(rt::default_timeout);
        }
    }
};

struct request_forwarder_t : public r::actor_base_t {
    using traits2_t = r::request_traits_t<req2_t>;
    using req_ptr_t = traits2_t::request::message_ptr_t;

    using r::actor_base_t::actor_base_t;

    int req_val = 0;
    int res_val = 0;
    r::address_ptr_t back_addr;
    r::request_id_t back_req1_id = 0;
    r::request_id_t back_req2_id = 0;
    req_ptr_t req_ptr;

    void configure(r::plugin_t &plugin) noexcept override {
        plugin.with_casted<r::internal::starter_plugin_t>([this](auto &p) {
            back_addr = supervisor->create_address();
            p.subscribe_actor(&request_forwarder_t::on_request_front);
            p.subscribe_actor(&request_forwarder_t::on_response_front);
            p.subscribe_actor(&request_forwarder_t::on_request_back, back_addr);
            p.subscribe_actor(&request_forwarder_t::on_response_back, back_addr);
        });
    }

    void shutdown_start() noexcept override {
        req_ptr.reset();
        r::actor_base_t::shutdown_start();
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        request<req2_t>(address, 4).send(rt::default_timeout);
    }

    void on_request_front(traits2_t::request::message_t &msg) noexcept {
        auto &payload = msg.payload.request_payload;
        back_req1_id = request_via<req2_t>(back_addr, back_addr, payload).send(r::pt::seconds(1));
        req_ptr = &msg;
    }

    void on_response_front(traits2_t::response::message_t &msg) noexcept {
        req_val += msg.payload.req->payload.request_payload.value;
        res_val += msg.payload.res->value;
    }

    void on_request_back(traits2_t::request::message_t &msg) noexcept { reply_to(msg, 5); }

    void on_response_back(traits2_t::response::message_t &msg) noexcept {
        req_val += msg.payload.req->payload.request_payload.value * 2;
        res_val += msg.payload.res->value * 2;
        back_req2_id = msg.payload.request_id();
        reply_to(*req_ptr, msg.payload.ec, std::move(msg.payload.res));
    }
};

struct intrusive_actor_t : public r::actor_base_t {
    using traits3_t = r::request_traits_t<req3_t>;
    using req_ptr_t = traits3_t::request::message_ptr_t;

    using r::actor_base_t::actor_base_t;

    int req_val = 0;
    int res_val = 0;
    r::address_ptr_t back_addr;
    req_ptr_t req_ptr;

    void configure(r::plugin_t &plugin) noexcept override {
        plugin.with_casted<r::internal::starter_plugin_t>([this](auto &p) {
            back_addr = supervisor->create_address();
            p.subscribe_actor(&intrusive_actor_t::on_request_front);
            p.subscribe_actor(&intrusive_actor_t::on_response_front);
            p.subscribe_actor(&intrusive_actor_t::on_request_back, back_addr);
            p.subscribe_actor(&intrusive_actor_t::on_response_back, back_addr);
        });
    }

    void shutdown_start() noexcept override {
        req_ptr.reset();
        r::actor_base_t::shutdown_start();
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        request<req3_t>(address, 4).send(r::pt::seconds(1));
    }

    void on_request_front(traits3_t::request::message_t &msg) noexcept {
        auto &payload = msg.payload.request_payload;
        request_via<req3_t>(back_addr, back_addr, payload).send(r::pt::seconds(1));
        req_ptr = &msg;
    }

    void on_response_front(traits3_t::response::message_t &msg) noexcept {
        req_val += msg.payload.req->payload.request_payload->value;
        res_val += msg.payload.res->value;
    }

    void on_request_back(traits3_t::request::message_t &msg) noexcept { reply_to(msg, 5); }

    void on_response_back(traits3_t::response::message_t &msg) noexcept {
        req_val += msg.payload.req->payload.request_payload->value * 2;
        res_val += msg.payload.res->value * 2;
        reply_to(*req_ptr, msg.payload.ec, std::move(msg.payload.res));
    }
};

struct duplicating_actor_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;
    int req_val = 0;
    int res_val = 0;
    std::error_code ec;

    void configure(r::plugin_t &plugin) noexcept override {
        plugin.with_casted<r::internal::starter_plugin_t>([](auto &p) {
            p.subscribe_actor(&duplicating_actor_t::on_request);
            p.subscribe_actor(&duplicating_actor_t::on_response);
        });
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        request<request_sample_t>(address, 4).send(rt::default_timeout);
    }

    void on_request(traits_t::request::message_t &msg) noexcept {
        reply_to(msg, 5);
        reply_to(msg, 5);
    }

    void on_response(traits_t::response::message_t &msg) noexcept {
        req_val += msg.payload.req->payload.request_payload.value;
        res_val += msg.payload.res.value;
        ec = msg.payload.ec;
    }
};

TEST_CASE("request-response successfull delivery", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    sup->do_process();

    auto init_subs_count = sup->get_subscription().handlers_count();
    auto init_pts_count = sup->get_points().size();

    auto actor = sup->create_actor<good_actor_t>().timeout(rt::default_timeout).finish();
    sup->do_process();

    REQUIRE(sup->active_timers.size() == 0);
    REQUIRE(actor->req_val == 4);
    REQUIRE(actor->res_val == 5);
    REQUIRE(actor->ec == r::error_code_t::success);

    actor->do_shutdown();
    sup->do_process();
    REQUIRE(sup->active_timers.size() == 0);
    std::size_t delta = 1; /* + shutdown confirmation triggered on self */
    REQUIRE(sup->get_points().size() == init_pts_count + delta);
    REQUIRE(sup->get_subscription().handlers_count() == init_subs_count + delta);

    sup->do_shutdown();
    sup->do_process();

    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().empty());
    REQUIRE(sup->get_children().size() == 0);
    REQUIRE(sup->get_requests().size() == 0);
    REQUIRE(sup->active_timers.size() == 0);
}

TEST_CASE("request-response successfull delivery indentical message to 2 actors", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto actor1 = sup->create_actor<good_actor_t>().timeout(rt::default_timeout).finish();
    auto actor2 = sup->create_actor<good_actor_t>().timeout(rt::default_timeout).finish();
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
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().empty());
    REQUIRE(sup->get_children().size() == 0);
    REQUIRE(sup->get_requests().size() == 0);
    REQUIRE(sup->active_timers.size() == 0);
}

TEST_CASE("request-response timeout", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto actor = sup->create_actor<bad_actor_t>().timeout(rt::default_timeout).finish();
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
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().empty());
    REQUIRE(sup->active_timers.size() == 0);
}

TEST_CASE("response with custom error", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto actor = sup->create_actor<bad_actor2_t>().timeout(rt::default_timeout).finish();
    sup->do_process();

    REQUIRE(actor->req_val == 4);
    REQUIRE(actor->res_val == 0);
    REQUIRE(actor->ec);
    REQUIRE(actor->ec == r::error_code_t::request_timeout);
    REQUIRE(sup->active_timers.size() == 0);

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().empty());
}

TEST_CASE("request-response successfull delivery (supervisor)", "[supervisor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<good_supervisor_t>().timeout(rt::default_timeout).finish();
    sup->do_process();

    REQUIRE(sup->active_timers.size() == 0);
    REQUIRE(sup->req_val == 4);
    REQUIRE(sup->res_val == 5);
    REQUIRE(sup->ec == r::error_code_t::success);

    sup->do_shutdown();
    sup->do_process();

    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().empty());
    REQUIRE(sup->get_children().size() == 0);
    REQUIRE(sup->get_requests().size() == 0);
    REQUIRE(sup->active_timers.size() == 0);
}

TEST_CASE("request-response successfull delivery, ref-counted response", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<good_supervisor_t>().timeout(rt::default_timeout).finish();
    auto actor = sup->create_actor<good_actor2_t>().timeout(rt::default_timeout).finish();
    sup->do_process();

    REQUIRE(sup->active_timers.size() == 0);
    REQUIRE(actor->req_val == 4);
    REQUIRE(actor->res_val == 5);
    REQUIRE(actor->ec == r::error_code_t::success);

    sup->do_shutdown();
    sup->do_process();

    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().empty());
    REQUIRE(sup->get_children().size() == 0);
    REQUIRE(sup->get_requests().size() == 0);
    REQUIRE(sup->active_timers.size() == 0);
}

TEST_CASE("request-response successfull delivery, twice", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<good_supervisor_t>().timeout(rt::default_timeout).finish();
    auto actor = sup->create_actor<good_actor3_t>().timeout(rt::default_timeout).finish();
    sup->do_process();

    REQUIRE(sup->active_timers.size() == 0);
    REQUIRE(actor->req_val == 4 * 2);
    REQUIRE(actor->res_val == 5 * 2);
    REQUIRE(actor->ec == r::error_code_t::success);

    sup->do_shutdown();
    sup->do_process();

    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().empty());
    REQUIRE(sup->get_children().size() == 0);
    REQUIRE(sup->get_requests().size() == 0);
    REQUIRE(sup->active_timers.size() == 0);
}

TEST_CASE("responce is sent twice, but received once", "[supervisor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<good_supervisor_t>().timeout(rt::default_timeout).finish();
    auto actor = sup->create_actor<duplicating_actor_t>().timeout(rt::default_timeout).finish();

    sup->do_process();
    REQUIRE(sup->active_timers.size() == 0);
    REQUIRE(actor->req_val == 4);
    REQUIRE(actor->res_val == 5);
    REQUIRE(actor->ec == r::error_code_t::success);

    sup->do_shutdown();
    sup->do_process();

    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().empty());
    REQUIRE(sup->get_children().size() == 0);
    REQUIRE(sup->get_requests().size() == 0);
    REQUIRE(sup->active_timers.size() == 0);
}

TEST_CASE("ref-counted response forwarding", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<good_supervisor_t>().timeout(rt::default_timeout).finish();
    auto actor = sup->create_actor<request_forwarder_t>().timeout(rt::default_timeout).finish();
    sup->do_process();

    REQUIRE(sup->active_timers.size() == 0);
    REQUIRE(actor->req_val == 4 + 4 * 2);
    REQUIRE(actor->res_val == 5 + 5 * 2);
    REQUIRE(actor->back_req1_id == actor->back_req2_id);

    sup->do_shutdown();
    sup->do_process();

    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().empty());
    REQUIRE(sup->get_children().size() == 0);
    REQUIRE(sup->get_requests().size() == 0);
    REQUIRE(sup->active_timers.size() == 0);
}

TEST_CASE("intrusive pointer request/responce", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<good_supervisor_t>().timeout(rt::default_timeout).finish();
    auto actor = sup->create_actor<intrusive_actor_t>().timeout(rt::default_timeout).finish();
    sup->do_process();

    REQUIRE(sup->active_timers.size() == 0);
    REQUIRE(actor->req_val == 4 + 4 * 2);
    REQUIRE(actor->res_val == 5 + 5 * 2);

    sup->do_shutdown();
    sup->do_process();

    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().empty());
    REQUIRE(sup->get_children().size() == 0);
    REQUIRE(sup->get_requests().size() == 0);
    REQUIRE(sup->active_timers.size() == 0);
}
