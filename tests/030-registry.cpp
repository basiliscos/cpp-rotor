//
// Copyright (c) 2019-2023 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "access.h"
#include "rotor.hpp"
#include "supervisor_test.h"
#include "actor_test.h"

namespace r = rotor;
namespace rt = r::test;

struct manual_actor_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;

    // no registry plugin
    // clang-format off
    using plugins_list_t = std::tuple<
        r::plugin::address_maker_plugin_t,
        r::plugin::lifetime_plugin_t,
        r::plugin::init_shutdown_plugin_t,
        r::plugin::starter_plugin_t>;
    // clang-format on

    using discovery_reply_t = r::intrusive_ptr_t<r::message::discovery_response_t>;
    using future_reply_t = r::intrusive_ptr_t<r::message::discovery_future_t>;
    using registration_reply_t = r::intrusive_ptr_t<r::message::registration_response_t>;

    r::address_ptr_t registry_addr;
    discovery_reply_t discovery_reply;
    future_reply_t future_reply;
    registration_reply_t registration_reply;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        r::actor_base_t::configure(plugin);
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) {
            p.subscribe_actor(&manual_actor_t::on_discovery);
            p.subscribe_actor(&manual_actor_t::on_registration_reply);
            p.subscribe_actor(&manual_actor_t::on_future);
        });
    }

    void query_name(const std::string &name) {
        request<r::payload::discovery_request_t>(registry_addr, name).send(rt::default_timeout);
    }

    r::request_id_t promise_name(const std::string &name) {
        return request<r::payload::discovery_promise_t>(registry_addr, name).send(rt::default_timeout);
    }

    void cancel_name(r::request_id_t request_id) {
        using payload_t = r::message::discovery_cancel_t::payload_t;
        send<payload_t>(registry_addr, request_id, address);
    }

    void register_name(const std::string &name) {
        request<r::payload::registration_request_t>(registry_addr, name, address).send(rt::default_timeout);
    }

    void unregister_all() { send<r::payload::deregistration_notify_t>(registry_addr, address); }

    void unregister_name(const std::string &name) { send<r::payload::deregistration_service_t>(registry_addr, name); }

    void on_discovery(r::message::discovery_response_t &reply) noexcept { discovery_reply.reset(&reply); }
    void on_future(r::message::discovery_future_t &reply) noexcept { future_reply.reset(&reply); }

    void on_registration_reply(r::message::registration_response_t &reply) noexcept {
        registration_reply.reset(&reply);
    }
};

struct sample_actor_t : rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;

    r::address_ptr_t service_addr;
};

TEST_CASE("supervisor related tests", "[registry][supervisor]") {
    r::system_context_t system_context;
    rt::supervisor_test_ptr_t sup;

    SECTION("no registry on supervisor by default") {
        sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
        sup->do_process();
        CHECK(!sup->access<rt::to::registry>());
    }

    SECTION("registry is created, when asked") {
        sup = system_context.create_supervisor<rt::supervisor_test_t>()
                  .timeout(rt::default_timeout)
                  .create_registry(true)
                  .finish();
        sup->do_process();
        CHECK(sup->access<rt::to::registry>());
    }

    SECTION("registry is inherited") {
        sup = system_context.create_supervisor<rt::supervisor_test_t>()
                  .timeout(rt::default_timeout)
                  .create_registry(true)
                  .finish();
        auto sup2 = sup->create_actor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();

        sup->do_process();

        CHECK(sup->access<rt::to::registry>());
        CHECK(sup2->access<rt::to::registry>());
    }

    SECTION("registry is set from different locality") {
        const char locality1[] = "abc";
        const char locality2[] = "def";
        sup = system_context.create_supervisor<rt::supervisor_test_t>()
                  .timeout(rt::default_timeout)
                  .locality(locality1)
                  .finish();
        auto reg = sup->create_actor<r::registry_t>().timeout(rt::default_timeout).finish();

        sup->do_process();
        CHECK(!sup->access<rt::to::registry>());

        auto sup2 = sup->create_actor<rt::supervisor_test_t>()
                        .timeout(rt::default_timeout)
                        .locality(locality2)
                        .registry_address(reg->get_address())
                        .finish();

        while (!sup->get_leader_queue().empty() || !sup2->get_leader_queue().empty()) {
            sup->do_process();
            sup2->do_process();
        }
        CHECK(sup2->access<rt::to::registry>());

        sup2->do_shutdown();
        while (!sup->get_leader_queue().empty() || !sup2->get_leader_queue().empty()) {
            sup->do_process();
            sup2->do_process();
        }
    }
    sup->do_shutdown();
    sup->do_process();
}

TEST_CASE("registry actor (server)", "[registry][supervisor]") {
    r::system_context_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>()
                   .timeout(rt::default_timeout)
                   .create_registry(true)
                   .finish();
    auto act = sup->create_actor<manual_actor_t>().timeout(rt::default_timeout).finish();
    act->registry_addr = sup->access<rt::to::registry>();
    sup->do_process();

    SECTION("discovery non-exsiting name") {
        act->query_name("some-name");
        sup->do_process();

        REQUIRE((bool)act->discovery_reply);
        auto &ec = act->discovery_reply->payload.ee->ec;
        CHECK(ec == r::error_code_t::unknown_service);
        CHECK(ec.message() == "the requested service name is not registered");
    }

    SECTION("duplicate registration attempt") {
        act->register_name("nnn");
        sup->do_process();
        REQUIRE((bool)act->registration_reply);
        REQUIRE(!act->registration_reply->payload.ee);

        act->register_name("nnn");
        sup->do_process();
        auto &ec = act->registration_reply->payload.ee->ec;
        REQUIRE((bool)ec);
        REQUIRE(ec == r::error_code_t::already_registered);
        REQUIRE(ec.message() == "service name is already registered");
    }

    SECTION("reg 2 names, check, unreg on, check") {
        act->register_name("s1");
        sup->do_process();
        REQUIRE((bool)act->registration_reply);
        REQUIRE(!act->registration_reply->payload.ee);

        act->query_name("s1");
        sup->do_process();
        REQUIRE((bool)act->discovery_reply);
        REQUIRE(!act->discovery_reply->payload.ee);
        REQUIRE(act->discovery_reply->payload.res.service_addr.get() == act->get_address().get());

        act->register_name("s2");
        sup->do_process();
        REQUIRE((bool)act->registration_reply);
        REQUIRE(!act->registration_reply->payload.ee);

        act->query_name("s2");
        sup->do_process();
        REQUIRE((bool)act->discovery_reply);
        REQUIRE(!act->discovery_reply->payload.ee);
        REQUIRE(act->discovery_reply->payload.res.service_addr.get() == act->get_address().get());

        act->register_name("s3");
        sup->do_process();
        REQUIRE((bool)act->registration_reply);
        REQUIRE(!act->registration_reply->payload.ee);

        act->unregister_name("s2");
        act->query_name("s2");
        sup->do_process();
        REQUIRE(act->discovery_reply->payload.ee->ec == r::error_code_t::unknown_service);

        act->unregister_all();
        act->query_name("s1");
        sup->do_process();
        REQUIRE(act->discovery_reply->payload.ee->ec == r::error_code_t::unknown_service);
        act->query_name("s3");
        sup->do_process();
        REQUIRE(act->discovery_reply->payload.ee->ec == r::error_code_t::unknown_service);
    }

    SECTION("promise & future") {
        REQUIRE(!act->future_reply);

        SECTION("promise, register, future") {
            act->promise_name("s1");
            act->register_name("s1");
            sup->do_process();
            CHECK(act->future_reply);
            CHECK(act->future_reply->payload.res.service_addr.get() == act->get_address().get());
        }
        SECTION("future, register, promise") {
            act->register_name("s1");
            act->promise_name("s1");
            sup->do_process();
            CHECK(act->future_reply);
            CHECK(act->future_reply->payload.res.service_addr.get() == act->get_address().get());
        }

        SECTION("cancel") {
            auto req_id = act->promise_name("s1");
            act->cancel_name(req_id);
            sup->do_process();
            auto id = &std::as_const(r::plugin::child_manager_plugin_t::class_identity);
            auto plugin = static_cast<r::actor_base_t *>(sup.get())->access<rt::to::get_plugin>(id);
            auto &reply = act->future_reply;
            CHECK(reply->payload.ee);
            CHECK(reply->payload.ee->ec.message() == "request has been cancelled");
            auto &actors_map = static_cast<r::plugin::child_manager_plugin_t *>(plugin)->access<rt::to::actors_map>();
            auto actor_state = actors_map.find(act->registry_addr);
            auto &registry = actor_state->second->actor;
            auto &promises = static_cast<r::registry_t *>(registry.get())->access<rt::to::promises>();
            CHECK(promises.empty());
        }
    }

    sup->do_shutdown();
    sup->do_process();
}

TEST_CASE("registry plugin (client)", "[registry][supervisor]") {
    r::system_context_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>()
                   .timeout(rt::default_timeout)
                   .create_registry(true)
                   .finish();
    SECTION("common case (just discover)") {
        auto act_s = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();
        act_s->configurer = [&](auto &actor, r::plugin::plugin_base_t &plugin) {
            plugin.with_casted<r::plugin::registry_plugin_t>(
                [&actor](auto &p) { p.register_name("service-name", actor.get_address()); });
        };

        sup->do_process();
        REQUIRE(sup->get_state() == r::state_t::OPERATIONAL);

        auto act_c = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();
        act_c->configurer = [&](auto &, r::plugin::plugin_base_t &plugin) {
            plugin.with_casted<r::plugin::registry_plugin_t>(
                [&](auto &p) { p.discover_name("service-name", act_c->service_addr); });
        };
        sup->do_process();
        CHECK(act_c->get_state() == r::state_t::OPERATIONAL);
        CHECK(act_c->service_addr == act_s->get_address());

        sup->do_shutdown();
        sup->do_process();
        CHECK(act_c->get_state() == r::state_t::SHUT_DOWN);
        CHECK(act_s->get_state() == r::state_t::SHUT_DOWN);
        CHECK(sup->get_state() == r::state_t::SHUT_DOWN);
    }

    SECTION("common case (discover & link)") {
        auto act_s = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();
        act_s->configurer = [&](auto &actor, r::plugin::plugin_base_t &plugin) {
            plugin.with_casted<r::plugin::registry_plugin_t>(
                [&actor](auto &p) { p.register_name("service-name", actor.get_address()); });
        };

        sup->do_process();
        REQUIRE(sup->get_state() == r::state_t::OPERATIONAL);

        auto act_c = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();
        int succeses = 0;
        act_c->configurer = [&](auto &, r::plugin::plugin_base_t &plugin) {
            plugin.with_casted<r::plugin::registry_plugin_t>([&](auto &p) {
                p.discover_name("service-name", act_c->service_addr)
                    .link(true)
                    .callback([&](auto /*phase*/, auto &ec) mutable {
                        REQUIRE(!ec);
                        ++succeses;
                    });
            });
        };
        sup->do_process();
        CHECK(act_c->get_state() == r::state_t::OPERATIONAL);
        CHECK(act_c->service_addr == act_s->get_address());
        CHECK(succeses == 2);

        sup->do_shutdown();
        sup->do_process();
        CHECK(act_c->get_state() == r::state_t::SHUT_DOWN);
        CHECK(act_s->get_state() == r::state_t::SHUT_DOWN);
        CHECK(sup->get_state() == r::state_t::SHUT_DOWN);
    }

    SECTION("aliasing (discover & link)") {
        auto act_s = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();
        act_s->configurer = [&](auto &actor, r::plugin::plugin_base_t &plugin) {
            plugin.with_casted<r::plugin::registry_plugin_t>([&actor](auto &p) {
                p.register_name("service-name", actor.get_address());
                p.register_name("service-alias", actor.get_address());
            });
        };

        sup->do_process();
        REQUIRE(sup->get_state() == r::state_t::OPERATIONAL);

        auto act_c = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();
        int succeses = 0;
        act_c->configurer = [&](auto &, r::plugin::plugin_base_t &plugin) {
            plugin.with_casted<r::plugin::registry_plugin_t>([&](auto &p) {
                p.discover_name("service-name", act_c->service_addr)
                    .link(true)
                    .callback([&](auto /*phase*/, auto &ec) mutable {
                        REQUIRE(!ec);
                        ++succeses;
                    });
                p.discover_name("service-alias", act_c->service_addr)
                    .link(true)
                    .callback([&](auto /*phase*/, auto &ec) mutable {
                        REQUIRE(!ec);
                        ++succeses;
                    });
            });
        };
        sup->do_process();
        CHECK(act_c->get_state() == r::state_t::OPERATIONAL);
        CHECK(act_c->service_addr == act_s->get_address());
        CHECK(succeses == 4);

        sup->do_shutdown();
        sup->do_process();
        CHECK(act_c->get_state() == r::state_t::SHUT_DOWN);
        CHECK(act_s->get_state() == r::state_t::SHUT_DOWN);
        CHECK(sup->get_state() == r::state_t::SHUT_DOWN);
    }

    SECTION("common case (promise & link)") {
        auto act_c = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();
        int succeses = 0;
        act_c->configurer = [&](auto &, r::plugin::plugin_base_t &plugin) {
            plugin.with_casted<r::plugin::registry_plugin_t>([&](auto &p) {
                p.discover_name("service-name", act_c->service_addr, true)
                    .link(true)
                    .callback([&](auto /*phase*/, auto &ec) mutable {
                        REQUIRE(!ec);
                        ++succeses;
                    });
            });
        };
        sup->do_process();
        CHECK(succeses == 0);

        SECTION("successful link") {
            auto act_s = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();
            act_s->configurer = [&](auto &actor, r::plugin::plugin_base_t &plugin) {
                plugin.with_casted<r::plugin::registry_plugin_t>(
                    [&actor](auto &p) { p.register_name("service-name", actor.get_address()); });
            };

            sup->do_process();
            CHECK(succeses == 2);
            CHECK(sup->get_state() == r::state_t::OPERATIONAL);
            CHECK(act_c->get_state() == r::state_t::OPERATIONAL);
            CHECK(act_c->service_addr == act_s->get_address());
        }

        SECTION("cancel promise") {
            CHECK(act_c->get_state() == r::state_t::INITIALIZING);
            act_c->do_shutdown();
            sup->do_process();
            CHECK(act_c->get_state() == r::state_t::SHUT_DOWN);
            auto id = &std::as_const(r::plugin::registry_plugin_t::class_identity);
            auto plugin = act_c->access<rt::to::get_plugin>(id);
            auto p = static_cast<r::plugin::registry_plugin_t *>(plugin);
            auto &dm = p->access<rt::to::discovery_map>();
            CHECK(dm.size() == 0);
        }

        sup->do_shutdown();
        sup->do_process();
        CHECK(act_c->get_state() == r::state_t::SHUT_DOWN);
        CHECK(sup->get_state() == r::state_t::SHUT_DOWN);
    }

    SECTION("discovery non-existing name => fail to init") {
        auto act = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();
        act->configurer = [&](auto &, r::plugin::plugin_base_t &plugin) {
            plugin.with_casted<r::plugin::registry_plugin_t>(
                [&act](auto &p) { p.discover_name("non-existing-service", act->service_addr); });
        };

        sup->do_process();
        REQUIRE(sup->get_state() == r::state_t::SHUT_DOWN);
        auto &reason = act->get_shutdown_reason();
        CHECK(reason->ec == r::shutdown_code_t::supervisor_shutdown);
        CHECK(reason->ec.message() == "actor shutdown has been requested by supervisor");
        CHECK(reason->next->ec == r::shutdown_code_t::child_init_failed);
        CHECK(reason->next->ec.message() == "supervisor shutdown due to child init failure");
        auto &down_reason = reason->next->next->next;
        REQUIRE(down_reason);
        CHECK(down_reason->ec == r::error_code_t::discovery_failed);
        CHECK(down_reason->ec.message() == "discovery has been failed");
    }

    SECTION("double name registration => fail") {
        auto act1 = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();
        auto act2 = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();
        printf("act1 = %p(%p), act2 = %p(%p)\n", (void *)act1.get(), (void *)act1->get_address().get(),
               (void *)act2.get(), (void *)act2->get_address().get());
        auto configurer = [](auto &actor, r::plugin::plugin_base_t &plugin) {
            plugin.with_casted<r::plugin::registry_plugin_t>(
                [&actor](auto &p) { p.register_name("service-name", actor.get_address()); });
        };
        act1->configurer = configurer;
        act2->configurer = configurer;

        sup->do_process();
        CHECK(act1->get_state() == r::state_t::SHUT_DOWN);
        CHECK(act2->get_state() == r::state_t::SHUT_DOWN);
        CHECK(sup->get_state() == r::state_t::SHUT_DOWN);

        auto &reason = act2->get_shutdown_reason();
        auto &down_reason = reason->next->next->next;
        REQUIRE(down_reason);
        CHECK(down_reason->ec == r::error_code_t::registration_failed);
        CHECK(down_reason->ec.message() == "registration has been failed");
    }
}

TEST_CASE("notify linked clients about going to shutdown", "[registry][supervisor]") {
    r::system_context_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>()
                   .timeout(rt::default_timeout)
                   .create_registry(true)
                   .finish();

    auto act1 = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();
    act1->configurer = [&](auto &, r::plugin::plugin_base_t &plugin) {
        plugin.with_casted<r::plugin::registry_plugin_t>([&act1](auto &p) {
            p.register_name("my-actor", act1->get_address());
            p.discover_name("non-existing-service", act1->service_addr, true);
        });
    };
    auto act2 = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();
    act2->configurer = [&](auto &, r::plugin::plugin_base_t &plugin) {
        plugin.with_casted<r::plugin::registry_plugin_t>(
            [&act1](auto &p) { p.discover_name("my-actor", act1->service_addr, true).link(false); });
    };

    sup->do_process();
    REQUIRE(act1->get_state() == r::state_t::INITIALIZING);
    REQUIRE(act2->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(sup->get_state() == r::state_t::INITIALIZING);

    act1->do_shutdown();
    sup->do_process();

    CHECK(act1->get_state() == r::state_t::SHUT_DOWN);
    CHECK(act2->get_state() == r::state_t::SHUT_DOWN);
    CHECK(sup->get_state() == r::state_t::SHUT_DOWN);
}

TEST_CASE("no problems when supervisor registers self in a registry", "[registry][supervisor]") {
    r::system_context_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>()
                   .timeout(rt::default_timeout)
                   .create_registry(true)
                   .configurer([](auto &actor, r::plugin::plugin_base_t &plugin) {
                       plugin.with_casted<r::plugin::registry_plugin_t>(
                           [&actor](auto &p) { p.register_name("service-name", actor.get_address()); });
                   })
                   .finish();

    SECTION("single supervisor and it's registry") {
        sup->do_process();
        CHECK(sup->get_state() == r::state_t::OPERATIONAL);
    }

    SECTION("supervisor + actor") {
        sup->do_process();
        CHECK(sup->get_state() == r::state_t::OPERATIONAL);

        auto act = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();
        act->configurer = [&](auto &, r::plugin::plugin_base_t &plugin) {
            plugin.with_casted<r::plugin::registry_plugin_t>(
                [&](auto &p) { p.discover_name("service-name", act->service_addr, false).link(false); });
        };

        sup->do_process();
        CHECK(act->get_state() == r::state_t::OPERATIONAL);
    }

    sup->do_shutdown();
    sup->do_process();
    CHECK(sup->get_state() == r::state_t::SHUT_DOWN);
}
