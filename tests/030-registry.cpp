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
    using discovery_reply_t = r::intrusive_ptr_t<r::message::discovery_response_t>;
    using registration_reply_t = r::intrusive_ptr_t<r::message::registration_response_t>;

    r::address_ptr_t registry_addr;
    discovery_reply_t discovery_reply;
    registration_reply_t registration_reply;

    void init_start() noexcept override {
        subscribe(&sample_actor_t::on_discovery);
        subscribe(&sample_actor_t::on_registration_reply);
        r::actor_base_t::init_start();
    }

    void query_name(const std::string &name) {
        auto timeout = r::pt::milliseconds{1};
        request<r::payload::discovery_request_t>(registry_addr, name).send(timeout);
    }

    void register_name(const std::string &name) {
        auto timeout = r::pt::milliseconds{1};
        request<r::payload::registration_request_t>(registry_addr, name, address).send(timeout);
    }

    void unregister_all() { send<r::payload::deregistration_notify_t>(registry_addr, address); }

    void unregister_name(const std::string &name) { send<r::payload::deregistration_service_t>(registry_addr, name); }

    void on_discovery(r::message::discovery_response_t &reply) noexcept { discovery_reply.reset(&reply); }

    void on_registration_reply(r::message::registration_response_t &reply) noexcept {
        registration_reply.reset(&reply);
    }
};

struct sample_supervisor_t : public rt::supervisor_test_t {
    using rt::supervisor_test_t::supervisor_test_t;

    void init_start() noexcept override {
        auto timeout = r::pt::milliseconds{10};
        registry = create_actor<r::registry_t>(timeout);
        rt::supervisor_test_t::init_start();
    }

    r::actor_ptr_t registry;
};

TEST_CASE("registry", "[registry]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto reg = sup->create_actor<r::registry_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();
    act->registry_addr = reg->get_address();

    sup->do_process();

    SECTION("discovery non-exsiting name") {
        act->query_name("some-name");
        sup->do_process();

        REQUIRE((bool)act->discovery_reply);
        REQUIRE(act->discovery_reply->payload.ec == r::make_error_code(r::error_code_t::unknown_service));
        REQUIRE(act->discovery_reply->payload.ec.message() == "the requested service name is not registered");
    }

    SECTION("duplicate registration attempt") {
        act->register_name("nnn");
        sup->do_process();
        REQUIRE((bool)act->registration_reply);
        REQUIRE(!act->registration_reply->payload.ec);

        act->register_name("nnn");
        sup->do_process();
        REQUIRE((bool)act->registration_reply->payload.ec);
        REQUIRE(act->registration_reply->payload.ec == r::make_error_code(r::error_code_t::already_registered));
        REQUIRE(act->registration_reply->payload.ec.message() == "service name is already registered");
    }

    SECTION("reg 2 names, check, unreg on, check") {
        act->register_name("s1");
        sup->do_process();
        REQUIRE((bool)act->registration_reply);
        REQUIRE(!act->registration_reply->payload.ec);

        act->query_name("s1");
        sup->do_process();
        REQUIRE((bool)act->discovery_reply);
        REQUIRE(!act->discovery_reply->payload.ec);
        REQUIRE(act->discovery_reply->payload.res.service_addr.get() == act->get_address().get());

        act->register_name("s2");
        sup->do_process();
        REQUIRE((bool)act->registration_reply);
        REQUIRE(!act->registration_reply->payload.ec);

        act->query_name("s2");
        sup->do_process();
        REQUIRE((bool)act->discovery_reply);
        REQUIRE(!act->discovery_reply->payload.ec);
        REQUIRE(act->discovery_reply->payload.res.service_addr.get() == act->get_address().get());

        act->register_name("s3");
        sup->do_process();
        REQUIRE((bool)act->registration_reply);
        REQUIRE(!act->registration_reply->payload.ec);

        act->unregister_name("s2");
        act->query_name("s2");
        sup->do_process();
        REQUIRE(act->discovery_reply->payload.ec == r::make_error_code(r::error_code_t::unknown_service));

        act->unregister_all();
        act->query_name("s1");
        sup->do_process();
        REQUIRE(act->discovery_reply->payload.ec == r::make_error_code(r::error_code_t::unknown_service));
        act->query_name("s3");
        sup->do_process();
        REQUIRE(act->discovery_reply->payload.ec == r::make_error_code(r::error_code_t::unknown_service));
    }

    sup->do_shutdown();
    sup->do_process();
}

TEST_CASE("common case for registry usage", "[registry]") {
    r::system_context_t system_context;

    auto timeout = r::pt::milliseconds{1};
    rt::supervisor_config_test_t config(timeout, nullptr);
    auto sup = system_context.create_supervisor<sample_supervisor_t>(nullptr, config);
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::OPERATIONAL);

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
}
