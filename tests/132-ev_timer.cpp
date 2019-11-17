//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "rotor/ev.hpp"
#include <ev.h>

namespace r = rotor;
namespace re = rotor::ev;
namespace pt = boost::posix_time;

struct sample_res_t {};
struct sample_req_t {
    using response_t = sample_res_t;
};

using traits_t = r::request_traits_t<sample_req_t>;

struct bad_actor_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;
    std::error_code ec;

    void init_start() noexcept override {
        subscribe(&bad_actor_t::on_response);
        r::actor_base_t::init_start();
    }

    void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        r::actor_base_t::on_start(msg);
        request<traits_t::request::type>(address).send(r::pt::milliseconds(1));
    }

    void on_response(traits_t::response::message_t &msg) noexcept {
        ec = msg.payload.ec;
        supervisor->do_shutdown();
    }
};

TEST_CASE("timer", "[supervisor][ev]") {
    auto *loop = ev_loop_new(0);
    auto system_context = r::intrusive_ptr_t<re::system_context_ev_t>{new re::system_context_ev_t()};
    auto timeout = r::pt::milliseconds{10};
    auto conf = re::supervisor_config_ev_t{nullptr, timeout, timeout, loop, true};
    auto sup = system_context->create_supervisor<re::supervisor_ev_t>(conf);
    auto actor = sup->create_actor<bad_actor_t>(conf);

    sup->start();
    ev_run(loop);

    REQUIRE(actor->ec == r::error_code_t::request_timeout);
    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
}
