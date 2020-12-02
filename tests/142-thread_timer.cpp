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
        start_timer(r::pt::milliseconds(1), *this, &bad_actor_t::delayed_start);
    }

    void delayed_start(r::request_id_t, bool) noexcept {
        request<traits_t::request::type>(address).send(r::pt::milliseconds(1));
    }

    void on_response(traits_t::response::message_t &msg) noexcept {
        ec = msg.payload.ec;
        supervisor->do_shutdown();
    }
};

TEST_CASE("timer", "[supervisor][thread]") {
    auto system_context = rth::system_context_thread_t();
    auto timeout = r::pt::milliseconds{10};
    auto sup = system_context.create_supervisor<rth::supervisor_thread_t>().timeout(timeout).finish();
    auto actor = sup->create_actor<bad_actor_t>().timeout(timeout).finish();

    sup->start();
    system_context.run();

    REQUIRE(actor->ec == r::error_code_t::request_timeout);
    REQUIRE(static_cast<r::actor_base_t *>(sup.get())->access<rt::to::state>() == r::state_t::SHUT_DOWN);
}
