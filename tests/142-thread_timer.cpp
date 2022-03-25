//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "rotor/thread.hpp"
#include "access.h"

namespace r = rotor;
namespace rth = rotor::thread;
namespace pt = boost::posix_time;
namespace rt = r::test;

namespace payload {
struct sample_res_t {};
struct sample_req_t {
    using response_t = sample_res_t;
};

struct trigger_t {};

} // namespace payload

namespace message {
using sample_req_t = r::request_traits_t<payload::sample_req_t>::request::message_t;
using sample_res_t = r::request_traits_t<payload::sample_req_t>::response::message_t;
using trigger_t = r::message_t<payload::trigger_t>;
} // namespace message

using req_ptr_t = r::intrusive_ptr_t<message::sample_req_t>;

struct bad_actor_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;
    r::extended_error_ptr_t ee;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        r::actor_base_t::configure(plugin);
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&bad_actor_t::on_response); });
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        // for coverage
        auto sup = static_cast<rth::supervisor_thread_t *>(supervisor);
        sup->update_time();
        start_timer(r::pt::milliseconds(1), *this, &bad_actor_t::delayed_start);
        start_timer(r::pt::minutes(1), *this, &bad_actor_t::delayed_start); // to be cancelled
    }

    void delayed_start(r::request_id_t, bool) noexcept {
        request<payload::sample_req_t>(address).send(r::pt::milliseconds(1));
    }

    void on_response(message::sample_res_t &msg) noexcept {
        ee = msg.payload.ee;
        supervisor->do_shutdown();
    }
};

struct io_actor1_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;

    r::extended_error_ptr_t ee;
    req_ptr_t req;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        r::actor_base_t::configure(plugin);
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) {
            p.subscribe_actor(&io_actor1_t::on_request);
            p.subscribe_actor(&io_actor1_t::on_response);
        });
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        request<payload::sample_req_t>(address).send(r::pt::milliseconds(9));
    }

    void on_request(message::sample_req_t &msg) noexcept {
        req = &msg;
        start_timer(r::pt::milliseconds(1), *this, &io_actor1_t::on_timeout);
    }

    void on_timeout(r::request_id_t, bool) noexcept { reply_to(*req); }

    void on_response(message::sample_res_t &msg) noexcept {
        ee = msg.payload.ee;
        supervisor->do_shutdown();
    }
};

struct io_actor2_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;

    r::extended_error_ptr_t ee;
    r::request_id_t req_id;
    std::uint32_t event_id = 0;
    std::uint32_t cancel_event;
    std::uint32_t timeout_event;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        r::actor_base_t::configure(plugin);
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) {
            p.subscribe_actor(&io_actor2_t::on_response);
            p.subscribe_actor(&io_actor2_t::on_trigger);
        });
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        start_timer(r::pt::milliseconds(100), *this, &io_actor2_t::on_timeout);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        send<payload::trigger_t>(address);
    }

    void on_trigger(message::trigger_t &) noexcept {
        req_id = request<payload::sample_req_t>(address).send(r::pt::milliseconds(70));
    }

    void on_timeout(r::request_id_t, bool) noexcept { cancel_event = ++event_id; }

    void on_response(message::sample_res_t &msg) noexcept {
        ee = msg.payload.ee;
        timeout_event = ++event_id;
        supervisor->do_shutdown();
    }
};

struct io_actor3_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;

    r::extended_error_ptr_t ee;
    r::request_id_t req_id;
    std::uint32_t event_id = 0;
    std::uint32_t cancel_event;
    std::uint32_t timeout_event;
    bool cancel_it = false;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        r::actor_base_t::configure(plugin);
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) {
            p.subscribe_actor(&io_actor3_t::on_response);
            p.subscribe_actor(&io_actor3_t::on_trigger)->tag_io();
        });
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        start_timer(r::pt::milliseconds(10), *this, &io_actor3_t::on_timeout);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        send<payload::trigger_t>(address);
    }

    void on_trigger(message::trigger_t &) noexcept {
        req_id = request<payload::sample_req_t>(address).send(r::pt::milliseconds(7));
        // for coverability
        auto id = start_timer(r::pt::milliseconds(10), *this, &io_actor3_t::dummy_timer);
        cancel_timer(id);
    }

    void on_timeout(r::request_id_t, bool) noexcept { cancel_event = ++event_id; }

    void dummy_timer(r::request_id_t, bool) noexcept {}

    void on_response(message::sample_res_t &msg) noexcept {
        ee = msg.payload.ee;
        timeout_event = ++event_id;
        supervisor->do_shutdown();
    }
};

TEST_CASE("timer", "[supervisor][thread]") {
    auto system_context = rth::system_context_thread_t();
    auto timeout = r::pt::milliseconds{100};
    auto sup = system_context.create_supervisor<rth::supervisor_thread_t>().timeout(timeout).finish();
    auto actor = sup->create_actor<bad_actor_t>().timeout(timeout).finish();

    sup->start();
    system_context.run();

    REQUIRE(actor->ee);
    REQUIRE(actor->ee->ec == r::error_code_t::request_timeout);
    REQUIRE(static_cast<r::actor_base_t *>(sup.get())->access<rt::to::state>() == r::state_t::SHUT_DOWN);
}

TEST_CASE("correct timeout triggering", "[supervisor][thread]") {
    auto system_context = rth::system_context_thread_t();
    auto timeout = r::pt::milliseconds{10};
    auto sup = system_context.create_supervisor<rth::supervisor_thread_t>().timeout(timeout).finish();
    auto actor = sup->create_actor<io_actor1_t>().timeout(timeout).finish();

    sup->start();
    system_context.run();

    REQUIRE(!actor->ee);
    REQUIRE(static_cast<r::actor_base_t *>(sup.get())->access<rt::to::state>() == r::state_t::SHUT_DOWN);
}

TEST_CASE("no I/O tag, incorrect timers", "[supervisor][thread]") {
    auto system_context = rth::system_context_thread_t();
    auto timeout = r::pt::milliseconds{100};
    auto sup = system_context.create_supervisor<rth::supervisor_thread_t>().timeout(timeout).finish();
    auto actor = sup->create_actor<io_actor2_t>().timeout(timeout).finish();

    sup->start();
    system_context.run();

    REQUIRE(actor->timeout_event == 1);
    REQUIRE(actor->cancel_event == 2);
    REQUIRE(actor->ee->ec == r::error_code_t::request_timeout);
    REQUIRE(static_cast<r::actor_base_t *>(sup.get())->access<rt::to::state>() == r::state_t::SHUT_DOWN);
}

TEST_CASE("has I/O tag, correct timers", "[supervisor][thread]") {
    auto system_context = rth::system_context_thread_t();
    auto timeout = r::pt::milliseconds{10};
    auto sup = system_context.create_supervisor<rth::supervisor_thread_t>().timeout(timeout).finish();
    auto actor = sup->create_actor<io_actor3_t>().timeout(timeout).finish();

    sup->start();
    system_context.run();

    REQUIRE(actor->cancel_event == 1);
    REQUIRE(actor->timeout_event == 2);
    REQUIRE(actor->ee->ec == r::error_code_t::request_timeout);
    REQUIRE(static_cast<r::actor_base_t *>(sup.get())->access<rt::to::state>() == r::state_t::SHUT_DOWN);
}
