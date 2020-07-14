//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
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
    std::error_code ec;

    void configure(r::plugin_t &plugin) noexcept override {
        r::actor_base_t::configure(plugin);
        plugin.with_casted<r::internal::starter_plugin_t>(
            [](auto &p) { p.subscribe_actor(&bad_actor_t::on_response); });
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        request<traits_t::request::type>(address).send(r::pt::milliseconds(1));
    }

    void on_response(traits_t::response::message_t &msg) noexcept {
        ec = msg.payload.ec;
        supervisor->do_shutdown();
    }
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

    REQUIRE(actor->ec == r::error_code_t::request_timeout);

    REQUIRE(actor->ec == r::error_code_t::request_timeout);
    REQUIRE(static_cast<r::actor_base_t *>(sup.get())->access<rt::to::state>() == r::state_t::SHUTTED_DOWN);
    CHECK(rt::empty(sup->get_subscription()));

    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    CHECK(rt::empty(sup->get_subscription()));
}
