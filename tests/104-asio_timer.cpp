//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor.hpp"
#include "rotor/asio.hpp"
#include "supervisor_asio_test.h"
#include "access.h"

namespace r = rotor;
namespace ra = rotor::asio;
namespace rt = r::test;
namespace asio = boost::asio;
namespace pt = boost::posix_time;

struct sample_res_t {};
struct sample_req_t {
    using response_t = sample_res_t;
};

using traits_t = r::request_traits_t<sample_req_t>;

struct bad_actor_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;
    r::extended_error_ptr_t ee;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        r::actor_base_t::configure(plugin);
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&bad_actor_t::on_response); });
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        start_timer(r::pt::milliseconds(1), *this, &bad_actor_t::delayed_start);
        start_timer(r::pt::minutes(1), *this, &bad_actor_t::delayed_start); // to be cancelled
    }

    void delayed_start(r::request_id_t, bool) noexcept {
        request<traits_t::request::type>(address).send(r::pt::milliseconds(1));
    }

    void on_response(traits_t::response::message_t &msg) noexcept {
        ee = msg.payload.ee;
        supervisor->do_shutdown();
    }
};

struct timer_actor_t: r::actor_base_t {
    using r::actor_base_t::actor_base_t;
    void on_start() noexcept override {
        r::actor_base_t::on_start();
        auto timeout = r::pt::time_duration{r::pt::seconds{-1}};
        auto req_id = start_timer(timeout, *this, &timer_actor_t::on_timer);
    }

    void on_timer(r::request_id_t req, bool cancelled){
        timer_triggered = true;
        do_shutdown();
    }

    bool timer_triggered = false;
};

TEST_CASE("timer", "[supervisor][asio]") {
    asio::io_context io_context{1};
    auto timeout = r::pt::milliseconds{10};
    auto system_context = ra::system_context_asio_t::ptr_t{new ra::system_context_asio_t(io_context)};
    auto strand = std::make_shared<asio::io_context::strand>(io_context);

    auto sup = system_context->create_supervisor<rt::supervisor_asio_test_t>().strand(strand).timeout(timeout).finish();
    auto actor = sup->create_actor<bad_actor_t>().timeout(timeout).finish();

    sup->start();
    io_context.run();

    REQUIRE(actor->ee->ec == r::error_code_t::request_timeout);

    REQUIRE(static_cast<r::actor_base_t *>(sup.get())->access<rt::to::state>() == r::state_t::SHUT_DOWN);
    CHECK(rt::empty(sup->get_subscription()));

    REQUIRE(sup->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    CHECK(rt::empty(sup->get_subscription()));
}

TEST_CASE("zero timeout", "[supervisor][asio]") {
    asio::io_context io_context{1};
    auto timeout = r::pt::milliseconds{10};
    auto system_context = ra::system_context_asio_t::ptr_t{new ra::system_context_asio_t(io_context)};
    auto strand = std::make_shared<asio::io_context::strand>(io_context);

    auto sup = system_context->create_supervisor<rt::supervisor_asio_test_t>().strand(strand).timeout(timeout).finish();
    auto actor = sup->create_actor<timer_actor_t>().timeout(timeout).autoshutdown_supervisor().finish();

    sup->start();
    io_context.run();

    CHECK(actor->timer_triggered);

    REQUIRE(static_cast<r::actor_base_t *>(actor.get())->access<rt::to::state>() == r::state_t::SHUT_DOWN);

    REQUIRE(sup->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    CHECK(rt::empty(sup->get_subscription()));
}
