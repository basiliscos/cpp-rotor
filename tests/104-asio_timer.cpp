//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "rotor/asio.hpp"
#include "supervisor_asio_test.h"

#include <iostream>

namespace r = rotor;
namespace ra = rotor::asio;
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
        request<traits_t::request::type>(address).send(r::pt::milliseconds(1));
    }

    void on_responce(traits_t::responce::message_t &msg) noexcept {
        ec = msg.payload.ec;
        supervisor.do_shutdown();
    }
};

TEST_CASE("timer", "[supervisor][asio]") {
    asio::io_context io_context{1};
    auto timeout = r::pt::milliseconds{10};
    auto system_context = ra::system_context_asio_t::ptr_t{new ra::system_context_asio_t(io_context)};
    auto stand = std::make_shared<asio::io_context::strand>(io_context);
    ra::supervisor_config_asio_t conf{timeout, std::move(stand)};
    auto sup = system_context->create_supervisor<rt::supervisor_asio_test_t>(conf);
    auto actor = sup->create_actor<bad_actor_t>(timeout);

    sup->start();
    io_context.run();

    REQUIRE(actor->ec == r::error_code_t::request_timeout);

    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().size() == 0);
}
