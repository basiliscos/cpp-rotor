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

struct sample_actor_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;
};

struct statuses_observer_t : public r::actor_base_t {
    r::address_ptr_t observable_addr;
    r::address_ptr_t dummy_addr;
    r::state_t observable_status;
    r::state_t dummy_status;
    r::state_t supervisor_status;
    r::state_t self_status;

    // statuses_observer_t()
    statuses_observer_t(r::supervisor_t &supervisor_) : r::actor_base_t(supervisor_) {
        observable_status = dummy_status = supervisor_status = r::state_t::UNKNOWN;
    }

    void on_initialize(r::message::init_request_t &msg) noexcept override {
        r::actor_base_t::on_initialize(msg);
        subscribe(&statuses_observer_t::on_state);
    }

    void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        r::actor_base_t::on_start(msg);
        auto sup_addr = supervisor.get_address();
        request<r::payload::state_request_t>(sup_addr, sup_addr).send(r::pt::seconds{1});
        request<r::payload::state_request_t>(sup_addr, dummy_addr).send(r::pt::seconds{1});
        request<r::payload::state_request_t>(sup_addr, observable_addr).send(r::pt::seconds{1});
        request<r::payload::state_request_t>(sup_addr, address).send(r::pt::seconds{1});
    }

    void on_state(r::message::state_response_t &msg) noexcept {
        auto &addr = msg.payload.req->payload.request_payload.subject_addr;
        auto &state = msg.payload.res.state;
        if (addr == supervisor.get_address()) {
            supervisor_status = state;
        } else if (addr == get_address()) {
            self_status = state;
        } else if (addr == dummy_addr) {
            dummy_status = state;
        } else if (addr == observable_addr) {
            observable_status = state;
        }
    }
};

TEST_CASE("statuses observer", "[actor]") {
    r::system_context_t system_context;

    auto timeout = r::pt::milliseconds{1};
    rt::supervisor_config_test_t config(timeout, nullptr);
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>(nullptr, config);
    auto observer = sup->create_actor<statuses_observer_t>(timeout);
    auto sample_actor = sup->create_actor<sample_actor_t>(timeout);
    observer->observable_addr = sample_actor->get_address();
    observer->dummy_addr = sup->create_address();

    sup->do_process();
    REQUIRE(observer->dummy_status == r::state_t::UNKNOWN);
    REQUIRE(observer->observable_status == r::state_t::OPERATIONAL);
    REQUIRE(observer->supervisor_status == r::state_t::OPERATIONAL);
    REQUIRE(observer->self_status == r::state_t::OPERATIONAL);

    sup->do_shutdown();
    sup->do_process();

    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().size() == 0);
}
