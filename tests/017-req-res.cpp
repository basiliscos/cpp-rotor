//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "supervisor_test.h"
#include "access.h"

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

struct notify_t {};

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
using req_ptr_t = r::intrusive_ptr_t<traits_t::request::message_t>;
using res_ptr_t = r::intrusive_ptr_t<traits_t::response::message_t>;
using notify_msg_t = r::message_t<notify_t>;

struct good_actor_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;
    int req_val = 0;
    int res_val = 0;
    r::extended_error_ptr_t ee;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) {
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
        ee = msg.payload.ee;
    }
};

struct bad_actor_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;
    int req_val = 0;
    int res_val = 0;
    r::extended_error_ptr_t ee;
    r::intrusive_ptr_t<traits_t::request::message_t> req_msg;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) {
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
        ee = msg.payload.ee;
        if (!ee) {
            res_val += 9;
        }
    }
};

struct bad_actor2_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;
    int req_val = 0;
    int res_val = 0;
    r::extended_error_ptr_t ee;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) {
            p.subscribe_actor(&bad_actor2_t::on_request);
            p.subscribe_actor(&bad_actor2_t::on_response);
        });
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        request<request_sample_t>(address, 4).send(rt::default_timeout);
    }

    void on_request(traits_t::request::message_t &msg) noexcept {
        auto ec = r::make_error_code(r::error_code_t::request_timeout);
        reply_with_error(msg, make_error(ec));
    }

    void on_response(traits_t::response::message_t &msg) noexcept {
        req_val += msg.payload.req->payload.request_payload.value;
        ee = msg.payload.ee;
        if (!ee) {
            res_val += 9;
        }
    }
};

struct good_supervisor_t : rt::supervisor_test_t {
    int req_val = 0;
    int res_val = 0;
    r::extended_error_ptr_t ee;

    using rt::supervisor_test_t::supervisor_test_t;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) {
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
        ee = msg.payload.ee;
    }
};

struct good_actor2_t : public r::actor_base_t {
    using traits2_t = r::request_traits_t<req2_t>;

    using r::actor_base_t::actor_base_t;

    int req_val = 0;
    int res_val = 0;
    r::address_ptr_t reply_addr;
    r::extended_error_ptr_t ee;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<r::plugin::starter_plugin_t>([this](auto &p) {
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
        ee = msg.payload.ee;
    }
};

struct good_actor3_t : public r::actor_base_t {
    using traits2_t = r::request_traits_t<req2_t>;

    using r::actor_base_t::actor_base_t;

    int req_left = 1;
    int req_val = 0;
    int res_val = 0;
    r::extended_error_ptr_t ee;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) {
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
        ee = msg.payload.ee;
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

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<r::plugin::starter_plugin_t>([this](auto &p) {
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
        reply_to(*req_ptr, msg.payload.ee, std::move(msg.payload.res));
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

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<r::plugin::starter_plugin_t>([this](auto &p) {
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
        reply_to(*req_ptr, msg.payload.ee, std::move(msg.payload.res));
    }
};

struct duplicating_actor_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;
    int req_val = 0;
    int res_val = 0;
    r::extended_error_ptr_t ee;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) {
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
        ee = msg.payload.ee;
    }
};

struct req_actor_t : r::actor_base_t {
    using r::actor_base_t::actor_base_t;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&req_actor_t::on_response); });
    }

    void do_request() { request<request_sample_t>(target, 4).send(rt::default_timeout); }

    void on_response(traits_t::response::message_t &msg) noexcept { res = &msg; }
    auto &get_state() noexcept { return state; }

    r::address_ptr_t target;
    res_ptr_t res;
};

struct res_actor_t : r::actor_base_t {
    using r::actor_base_t::actor_base_t;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&res_actor_t::on_request); });
    }

    void on_request(traits_t::request::message_t &msg) noexcept { req = &msg; }

    auto &get_state() noexcept { return state; }

    req_ptr_t req;
};

struct order_actor_t : r::actor_base_t {
    using r::actor_base_t::actor_base_t;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<r::plugin::starter_plugin_t>([&](auto &p) {
            order = 5;
            p.subscribe_actor(&order_actor_t::on_request);
            p.subscribe_actor(&order_actor_t::on_response);
            p.subscribe_actor(&order_actor_t::on_notify);
        });
    }

    void on_start() noexcept override { request<request_sample_t>(address).send(rt::default_timeout); }

    void on_request(traits_t::request::message_t &msg) noexcept {
        reply_to(msg);
        send<notify_t>(address);
    }

    void on_response(traits_t::response::message_t &) noexcept { order *= 10; }

    void on_notify(notify_msg_t &) noexcept { order += 3; }

    int order;
};

TEST_CASE("request-response successfull delivery", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    sup->do_process();

    auto init_subs_count = sup->get_subscription().access<rt::to::mine_handlers>().size();
    auto init_pts_count = sup->get_points().size();

    auto actor = sup->create_actor<good_actor_t>().timeout(rt::default_timeout).finish();
    sup->do_process();

    REQUIRE(sup->active_timers.size() == 0);
    REQUIRE(actor->req_val == 4);
    REQUIRE(actor->res_val == 5);
    CHECK(!actor->ee);

    actor->do_shutdown();
    sup->do_process();
    REQUIRE(sup->active_timers.size() == 0);
    std::size_t delta = 1; /* + shutdown confirmation triggered on self */
    REQUIRE(sup->get_points().size() == init_pts_count + delta);
    REQUIRE(sup->get_subscription().access<rt::to::mine_handlers>().size() == init_subs_count + delta);

    sup->do_shutdown();
    sup->do_process();

    REQUIRE(sup->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    CHECK(rt::empty(sup->get_subscription()));
    REQUIRE(sup->get_children_count() == 0);
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
    CHECK(!actor1->ee);
    REQUIRE(actor2->req_val == 4);
    REQUIRE(actor2->res_val == 5);
    CHECK(!actor2->ee);

    sup->do_shutdown();
    sup->do_process();

    REQUIRE(sup->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    CHECK(rt::empty(sup->get_subscription()));
    REQUIRE(sup->get_children_count() == 0);
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
    REQUIRE(!actor->ee);

    auto timer_it = *sup->active_timers.begin();
    ((r::actor_base_t *)sup.get())
        ->access<rt::to::on_timer_trigger, r::request_id_t, bool>(timer_it->request_id, false);
    sup->do_process();
    REQUIRE(actor->req_msg);
    REQUIRE(actor->req_val == 4);
    REQUIRE(actor->res_val == 0);
    REQUIRE(actor->ee);
    REQUIRE(actor->ee->ec == r::error_code_t::request_timeout);
    REQUIRE(actor->ee->ec.message() == std::string("request timeout"));

    sup->active_timers.clear();

    actor->reply_to(*actor->req_msg, 1);
    sup->do_process();
    // nothing should be changed, i.e. reply should just be dropped
    REQUIRE(actor->req_val == 4);
    REQUIRE(actor->res_val == 0);
    REQUIRE(actor->ee->ec == r::error_code_t::request_timeout);

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    CHECK(rt::empty(sup->get_subscription()));
    REQUIRE(sup->active_timers.size() == 0);
}

TEST_CASE("response with custom error", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto actor = sup->create_actor<bad_actor2_t>().timeout(rt::default_timeout).finish();
    sup->do_process();

    REQUIRE(actor->req_val == 4);
    REQUIRE(actor->res_val == 0);
    REQUIRE(actor->ee);
    REQUIRE(actor->ee->ec == r::error_code_t::request_timeout);
    REQUIRE(sup->active_timers.size() == 0);

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    CHECK(rt::empty(sup->get_subscription()));
}

TEST_CASE("request-response successfull delivery (supervisor)", "[supervisor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<good_supervisor_t>().timeout(rt::default_timeout).finish();
    sup->do_process();

    REQUIRE(sup->active_timers.size() == 0);
    REQUIRE(sup->req_val == 4);
    REQUIRE(sup->res_val == 5);
    CHECK(!sup->ee);

    sup->do_shutdown();
    sup->do_process();

    REQUIRE(sup->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    CHECK(rt::empty(sup->get_subscription()));
    REQUIRE(sup->get_children_count() == 0);
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
    CHECK(!actor->ee);

    sup->do_shutdown();
    sup->do_process();

    REQUIRE(sup->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    CHECK(rt::empty(sup->get_subscription()));
    REQUIRE(sup->get_children_count() == 0);
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
    CHECK(!actor->ee);

    sup->do_shutdown();
    sup->do_process();

    REQUIRE(sup->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    CHECK(rt::empty(sup->get_subscription()));
    REQUIRE(sup->get_children_count() == 0);
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
    CHECK(!actor->ee);

    sup->do_shutdown();
    sup->do_process();

    REQUIRE(sup->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    CHECK(rt::empty(sup->get_subscription()));
    REQUIRE(sup->get_children_count() == 0);
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

    REQUIRE(sup->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    CHECK(rt::empty(sup->get_subscription()));
    REQUIRE(sup->get_children_count() == 0);
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

    REQUIRE(sup->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    CHECK(rt::empty(sup->get_subscription()));
    REQUIRE(sup->get_children_count() == 0);
    REQUIRE(sup->get_requests().size() == 0);
    REQUIRE(sup->active_timers.size() == 0);
}

TEST_CASE("response arrives after requestee shutdown", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto req = sup->create_actor<req_actor_t>().timeout(rt::default_timeout).finish();
    auto res = sup->create_actor<res_actor_t>().timeout(rt::default_timeout).finish();
    sup->do_process();

    REQUIRE(sup->get_state() == r::state_t::OPERATIONAL);

    req->target = res->get_address();
    req->do_request();
    sup->do_process();

    REQUIRE(!req->res);
    REQUIRE(res->req);

    req->do_shutdown();
    sup->do_process();
    REQUIRE(req->get_state() == r::state_t::SHUT_DOWN);

    res->reply_to(*res->req, 5);
    sup->do_process();
    REQUIRE(!req->res);

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUT_DOWN);
}

TEST_CASE("response arrives after requestee shutdown (on the same localities)", "[actor]") {
    r::system_context_t system_context;

    auto sup1 = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto sup2 = sup1->create_actor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto req = sup2->create_actor<req_actor_t>().timeout(rt::default_timeout).finish();
    auto res = sup1->create_actor<res_actor_t>().timeout(rt::default_timeout).finish();
    sup1->do_process();

    REQUIRE(sup1->get_state() == r::state_t::OPERATIONAL);

    req->target = res->get_address();
    req->do_request();
    sup1->do_process();

    REQUIRE(!req->res);
    REQUIRE(res->req);

    req->do_shutdown();
    sup1->do_process();
    REQUIRE(req->get_state() == r::state_t::SHUT_DOWN);

    res->reply_to(*res->req, 5);
    sup1->do_process();
    REQUIRE(!req->res);

    sup1->do_shutdown();
    sup1->do_process();
    REQUIRE(sup1->get_state() == r::state_t::SHUT_DOWN);
}

TEST_CASE("request timer should not outlive requestee", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto req = sup->create_actor<req_actor_t>().timeout(rt::default_timeout).finish();
    sup->do_process();

    auto act = sup->create_actor<r::actor_base_t>().timeout(rt::default_timeout).finish();
    sup->do_process();

    act->request<request_sample_t>(sup->get_address(), 5).send(rt::default_timeout);
    CHECK(!act->access<rt::to::active_requests>().empty());

    act->do_shutdown();
    sup->do_process();

    CHECK(act->access<rt::to::active_requests>().empty());

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUT_DOWN);
}

TEST_CASE("response and regual messages keep send order", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<order_actor_t>().timeout(rt::default_timeout).finish();
    sup->do_process();

    CHECK(act->order == 53);

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUT_DOWN);
}
