//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "actor_test.h"
#include "supervisor_test.h"
#include "system_context_test.h"
#include "access.h"

namespace r = rotor;
namespace rt = r::test;

struct double_linked_actor_t : r::actor_base_t {
    using r::actor_base_t::actor_base_t;
    using message_ptr_t = r::intrusive_ptr_t<r::message::link_response_t>;

    struct resource {
        static const constexpr r::plugin::resource_id_t linking = 0;
        static const constexpr r::plugin::resource_id_t unlinking = 1;
    };

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<r::plugin::address_maker_plugin_t>([&](auto &p) { alternative = p.create_address(); });
        plugin.with_casted<r::plugin::starter_plugin_t>([&](auto &p) {
            p.subscribe_actor(&double_linked_actor_t::on_link, alternative);
            p.subscribe_actor(&double_linked_actor_t::on_unlink, alternative);
            for (auto i = 0; i < 2; ++i) {
                resources->acquire(resource::linking);
                request_via<r::payload::link_request_t>(target, alternative, false).send(rt::default_timeout);
            }
        });
    }

    void on_link(r::message::link_response_t &res) noexcept {
        resources->release(resource::linking);
        if (!message1)
            message1 = &res;
        else if (!message2)
            message2 = &res;
    }

    virtual void on_start() noexcept override {
        r::actor_base_t::on_start();
        resources->acquire(resource::unlinking);
    }

    void on_unlink(r::message::unlink_request_t &message) noexcept {
        reply_to(message, alternative);
        if (resources->has(resource::unlinking))
            resources->release(resource::unlinking);
    }

    r::address_ptr_t target;
    message_ptr_t message1, message2;
    r::address_ptr_t alternative;
};

struct tracked_actor_t : rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;
    std::uint32_t shutdown_event = 0;
};

TEST_CASE("client/server, common workflow", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act_s = sup->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto act_c = sup->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto &addr_s = act_s->get_address();

    bool invoked = false;
    act_c->configurer = [&](auto &, r::plugin::plugin_base_t &plugin) {
        plugin.with_casted<r::plugin::link_client_plugin_t>([&](auto &p) {
            p.link(addr_s, false, [&](auto &ec) mutable {
                REQUIRE(ec.category().name() == std::string("rotor_error"));
                REQUIRE(ec.message() == std::string("success"));
                invoked = true;
            });
        });
    };
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(invoked);

    SECTION("simultaneous shutdown") {
        sup->do_shutdown();
        sup->do_process();
    }

    SECTION("controlled shutdown") {
        SECTION("indirect client-initiated unlink via client-shutdown") {
            act_c->do_shutdown();
            sup->do_process();
            CHECK(act_c->get_state() == r::state_t::SHUT_DOWN);
        }

        SECTION("indirect client-initiated unlink via server-shutdown") {
            act_s->do_shutdown();
            sup->do_process();
            CHECK(act_s->get_state() == r::state_t::SHUT_DOWN);
            CHECK(act_c->get_state() == r::state_t::SHUT_DOWN);
        }

        sup->do_shutdown();
        sup->do_process();
    }
}

TEST_CASE("link not possible (timeout) => shutdown", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act_c = sup->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto some_addr = sup->make_address();

    bool invoked = false;
    act_c->configurer = [&](auto &, r::plugin::plugin_base_t &plugin) {
        plugin.with_casted<r::plugin::link_client_plugin_t>([&](auto &p) {
            p.link(some_addr, false, [&](auto &ec) mutable {
                REQUIRE(ec);
                invoked = true;
            });
        });
    };
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::INITIALIZING);

    REQUIRE(sup->active_timers.size() == 3);
    auto timer_it = *(sup->active_timers.rbegin());
    sup->do_invoke_timer(timer_it->request_id);

    sup->do_process();
    REQUIRE(invoked);
    REQUIRE(sup->get_state() == r::state_t::SHUT_DOWN);
}

#if 0
TEST_CASE("link not possible => supervisor is shutted down", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act_s = sup->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto act_c = sup->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();

    auto server_addr = act_s->get_address();

    act_c->link_request(server_addr, rt::default_timeout);
    sup->do_process();

    REQUIRE(act_c->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(act_s->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup->get_state() == r::state_t::SHUT_DOWN);
}
#endif

TEST_CASE("unlink", "[actor]") {
    rt::system_context_test_t system_context;

    const char l1[] = "abc";
    const char l2[] = "def";

    auto sup1 =
        system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).locality(l1).finish();
    auto sup2 = sup1->create_actor<rt::supervisor_test_t>().timeout(rt::default_timeout).locality(l2).finish();

    auto act_s = sup1->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto act_c = sup2->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto &addr_s = act_s->get_address();

    act_c->configurer = [&](auto &, r::plugin::plugin_base_t &plugin) {
        plugin.with_casted<r::plugin::link_client_plugin_t>([&](auto &p) { p.link(addr_s, false, [&](auto &) {}); });
    };
    while (!sup1->get_leader_queue().empty() || !sup2->get_leader_queue().empty()) {
        sup1->do_process();
        sup2->do_process();
    }
    REQUIRE(sup1->get_state() == r::state_t::OPERATIONAL);

    SECTION("unlink failure") {
        act_s->do_shutdown();
        sup1->do_process();
        REQUIRE(sup1->active_timers.size() == 2);

        auto unlink_req = sup1->get_timer(1);
        sup1->do_invoke_timer(unlink_req);
        sup1->do_process();

        REQUIRE(system_context.reason == r::error_code_t::request_timeout);
        REQUIRE(act_s->get_state() == r::state_t::SHUTTING_DOWN);
        act_s->force_cleanup();
    }

    SECTION("unlink-notify on unlink-request") {
        SECTION("client, then server") {
            act_s->do_shutdown();
            act_c->do_shutdown();
            sup2->do_process();
            sup1->do_process();
            sup2->do_process();
            sup1->do_process();
        }

        SECTION("server, then client") {
            act_s->do_shutdown();
            act_c->do_shutdown();
            sup1->do_process();
            sup2->do_process();
            sup1->do_process();
            sup2->do_process();
        }
    }

    sup1->do_shutdown();
    while (!sup1->get_leader_queue().empty() || !sup2->get_leader_queue().empty()) {
        sup1->do_process();
        sup2->do_process();
    }
    REQUIRE(sup1->get_state() == r::state_t::SHUT_DOWN);
}

TEST_CASE("unlink reaction", "[actor]") {
    using request_ptr_t = r::intrusive_ptr_t<r::message::unlink_request_t>;
    rt::system_context_test_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();

    auto act_s = sup->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto act_c = sup->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto &addr_s = act_s->get_address();

    request_ptr_t unlink_req;
    act_c->configurer = [&](auto &, r::plugin::plugin_base_t &plugin) {
        plugin.with_casted<r::plugin::link_client_plugin_t>([&](auto &p) {
            p.link(addr_s, false, [&](auto &) {});
            p.on_unlink([&](auto &req) {
                unlink_req = &req;
                p.forget_link(req);
                return true;
            });
        });
    };

    sup->do_process();
    act_s->do_shutdown();
    sup->do_process();

    REQUIRE(unlink_req);
    REQUIRE(unlink_req->message_type == r::message::unlink_request_t::message_type);

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUT_DOWN);
}

TEST_CASE("auto-unlink on shutdown", "[actor]") {
    rt::system_context_test_t ctx1;
    rt::system_context_test_t ctx2;

    const char l1[] = "abc";
    const char l2[] = "def";

    auto sup1 = ctx1.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).locality(l1).finish();
    auto sup2 = ctx2.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).locality(l2).finish();

    auto act_c = sup1->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto act_s = sup2->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto &addr_s = act_s->get_address();

    act_c->configurer = [&](auto &, r::plugin::plugin_base_t &plugin) {
        plugin.with_casted<r::plugin::link_client_plugin_t>([&](auto &p) { p.link(addr_s, false, [&](auto &) {}); });
    };

    sup1->do_process();
    REQUIRE(act_c->get_state() == r::state_t::INITIALIZING);
    act_c->do_shutdown();

    sup1->do_process();
    REQUIRE(act_c->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup1->get_state() == r::state_t::SHUT_DOWN);

    sup2->do_process();
    REQUIRE(sup2->get_state() == r::state_t::OPERATIONAL);

    sup2->do_shutdown();
    sup2->do_process();
    REQUIRE(sup2->get_state() == r::state_t::SHUT_DOWN);
}

TEST_CASE("link to operational only", "[actor]") {
    rt::system_context_test_t ctx1;
    rt::system_context_test_t ctx2;
    rt::system_context_test_t ctx3;

    const char l1[] = "abc";
    const char l2[] = "def";
    const char l3[] = "ghi";

    auto sup1 = ctx1.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).locality(l1).finish();
    auto sup2 = ctx2.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).locality(l2).finish();
    auto sup3 = ctx3.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).locality(l3).finish();

    auto act_c = sup1->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto act_s1 = sup2->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto act_s2 = sup3->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();

    auto &addr_s1 = act_s1->get_address();
    auto &addr_s2 = act_s2->get_address();

    auto process_12 = [&]() {
        while (!sup1->get_leader_queue().empty() || !sup2->get_leader_queue().empty()) {
            sup1->do_process();
            sup2->do_process();
        }
    };
    auto process_123 = [&]() {
        while (!sup1->get_leader_queue().empty() || !sup2->get_leader_queue().empty() ||
               !sup3->get_leader_queue().empty()) {
            sup1->do_process();
            sup2->do_process();
            sup3->do_process();
        }
    };

    act_c->configurer = [&](auto &, r::plugin::plugin_base_t &plugin) {
        plugin.with_casted<r::plugin::link_client_plugin_t>([&](auto &p) { p.link(addr_s1, true, [&](auto &) {}); });
    };
    act_s1->configurer = [&](auto &, r::plugin::plugin_base_t &plugin) {
        plugin.with_casted<r::plugin::link_client_plugin_t>([&](auto &p) { p.link(addr_s2, true, [&](auto &) {}); });
    };

    process_12();
    CHECK(act_c->get_state() == r::state_t::INITIALIZING);
    CHECK(act_s1->get_state() == r::state_t::INITIALIZING);

    process_123();
    CHECK(act_c->get_state() == r::state_t::OPERATIONAL);
    CHECK(act_s1->get_state() == r::state_t::OPERATIONAL);
    CHECK(act_s2->get_state() == r::state_t::OPERATIONAL);

    sup1->do_shutdown();
    sup2->do_shutdown();
    sup3->do_shutdown();
    process_123();

    CHECK(act_c->get_state() == r::state_t::SHUT_DOWN);
    CHECK(act_s1->get_state() == r::state_t::SHUT_DOWN);
    CHECK(act_s2->get_state() == r::state_t::SHUT_DOWN);
}

TEST_CASE("unlink notify / response race", "[actor]") {
    rt::system_context_test_t system_context;

    const char l1[] = "abc";
    const char l2[] = "def";

    auto sup1 =
        system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).locality(l1).finish();
    auto sup2 = sup1->create_actor<rt::supervisor_test_t>().timeout(rt::default_timeout).locality(l2).finish();

    auto act_s = sup1->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto act_c = sup2->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto &addr_s = act_s->get_address();

    act_c->configurer = [&](auto &, r::plugin::plugin_base_t &plugin) {
        plugin.with_casted<r::plugin::link_client_plugin_t>([&](auto &p) { p.link(addr_s, true, [&](auto &) {}); });
    };
    while (!sup1->get_leader_queue().empty() || !sup2->get_leader_queue().empty()) {
        sup1->do_process();
        sup2->do_process();
    }
    REQUIRE(sup1->get_state() == r::state_t::OPERATIONAL);

    act_s->do_shutdown();
    act_c->do_shutdown();
    sup1->do_process();

    // extract unlink request to let it produce unlink notify
    auto unlink_request = sup2->get_leader_queue().back();
    REQUIRE(unlink_request->type_index == r::message::unlink_request_t::message_type);
    sup2->get_leader_queue().pop_back();
    sup2->do_process();

    sup1->do_shutdown();
    while (!sup1->get_leader_queue().empty() || !sup2->get_leader_queue().empty()) {
        sup1->do_process();
        sup2->do_process();
    }
    CHECK(sup1->active_timers.size() == 0);
    CHECK(sup1->get_state() == r::state_t::SHUT_DOWN);
}

TEST_CASE("link errors", "[actor]") {
    rt::system_context_test_t ctx1;
    rt::system_context_test_t ctx2;

    const char l1[] = "abc";
    const char l2[] = "def";

    auto sup1 = ctx1.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).locality(l1).finish();
    auto sup2 = ctx2.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).locality(l2).finish();

    auto process_12 = [&]() {
        while (!sup1->get_leader_queue().empty() || !sup2->get_leader_queue().empty()) {
            sup1->do_process();
            sup2->do_process();
        }
    };

    SECTION("double link attempt") {
        auto act_c = sup1->create_actor<double_linked_actor_t>().timeout(rt::default_timeout).finish();
        auto act_s = sup2->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();

        act_c->target = act_s->get_address();

        process_12();
        REQUIRE(act_c->message1);
        CHECK(!act_c->message1->payload.ec);

        REQUIRE(act_c->message2);
        CHECK(act_c->message2->payload.ec);
        CHECK(act_c->message2->payload.ec.message() == std::string("already linked"));
    }

    SECTION("not linkeable") {
        auto act_s = sup2->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
        sup2->do_process();

        act_s->access<rt::to::resources>()->acquire();
        act_s->do_shutdown();
        sup2->do_process();
        REQUIRE(act_s->get_state() == r::state_t::SHUTTING_DOWN);

        SECTION("check error") {
            std::error_code err;
            auto act_c = sup1->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
            act_c->configurer = [&](auto &, r::plugin::plugin_base_t &plugin) {
                plugin.with_casted<r::plugin::link_client_plugin_t>(
                    [&](auto &p) { p.link(act_s->get_address(), false, [&](auto &ec) { err = ec; }); });
            };
            process_12();
            CHECK(act_c->get_state() == r::state_t::SHUT_DOWN);
            CHECK(err.message() == std::string("actor is not linkeable"));
        }

        SECTION("get the error during shutdown") {
            auto act_c = sup1->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
            sup1->do_process();
            CHECK(act_c->get_state() == r::state_t::OPERATIONAL);

            auto plugin1 = act_c->access<rt::to::get_plugin>(r::plugin::link_client_plugin_t::class_identity);
            auto p1 = static_cast<r::plugin::link_client_plugin_t *>(plugin1);
            p1->link(act_s->get_address(), false, [&](auto &) {});
            act_c->access<rt::to::resources>()->acquire();
            act_c->do_shutdown();

            process_12();
            CHECK(act_c->get_state() == r::state_t::SHUTTING_DOWN);
            act_c->access<rt::to::resources>()->release();
        }

        act_s->access<rt::to::resources>()->release();
    }

    SECTION("unlink during shutring down") {
        auto act_c = sup1->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
        auto act_s = sup2->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();

        act_c->configurer = [&](auto &, r::plugin::plugin_base_t &plugin) {
            plugin.with_casted<r::plugin::link_client_plugin_t>(
                [&](auto &p) { p.link(act_s->get_address(), false, [&](auto &) {}); });
        };

        process_12();
        CHECK(sup1->get_state() == r::state_t::OPERATIONAL);
        CHECK(sup2->get_state() == r::state_t::OPERATIONAL);

        act_c->do_shutdown();
        act_c->access<rt::to::resources>()->acquire();
        sup1->do_process();
        CHECK(act_c->get_state() == r::state_t::SHUTTING_DOWN);

        act_s->do_shutdown();
        sup2->do_process();

        sup1->do_process();
        CHECK(act_c->get_state() == r::state_t::SHUTTING_DOWN);
        act_c->access<rt::to::resources>()->release();
        process_12();
    }

    sup1->do_shutdown();
    sup2->do_shutdown();
    process_12();

    CHECK(sup1->get_state() == r::state_t::SHUT_DOWN);
    CHECK(sup2->get_state() == r::state_t::SHUT_DOWN);
}

TEST_CASE("proper shutdown order, defined by linkage", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act_1 = sup->create_actor<tracked_actor_t>().timeout(rt::default_timeout).finish();
    auto act_2 = sup->create_actor<tracked_actor_t>().timeout(rt::default_timeout).finish();
    auto act_3 = sup->create_actor<tracked_actor_t>().timeout(rt::default_timeout).finish();

    /*
    printf("a1 = %p(%p), a2 = %p(%p), a3 = %p(%p)\n", act_1.get(), act_1->get_address().get(),
           act_2.get(), act_2->get_address().get(), act_3.get(), act_3->get_address().get());
    */

    std::uint32_t event_id = 1;
    auto shutdowner = [&](auto &me) {
        auto &self = static_cast<tracked_actor_t &>(me);
        self.shutdown_event = event_id++;
    };
    act_1->shutdowner = act_2->shutdowner = act_3->shutdowner = shutdowner;

    act_1->configurer = [&](auto &, r::plugin::plugin_base_t &plugin) {
        plugin.with_casted<r::plugin::link_client_plugin_t>([&](auto &p) {
            p.link(act_2->get_address(), false);
            p.link(act_3->get_address(), false);
        });
    };
    act_2->configurer = [&](auto &, r::plugin::plugin_base_t &plugin) {
        plugin.with_casted<r::plugin::link_client_plugin_t>([&](auto &p) { p.link(act_3->get_address(), false); });
    };
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::OPERATIONAL);

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUT_DOWN);

    CHECK(act_1->shutdown_event == 1);
    CHECK(act_2->shutdown_event == 2);
    CHECK(act_3->shutdown_event == 3);
}

TEST_CASE("unlink of root supervisor", "[actor]") {
    rt::system_context_test_t ctx;

    rt::system_context_test_t ctx1;
    rt::system_context_test_t ctx2;

    const char l1[] = "abc";
    const char l2[] = "def";

    auto sup1 = ctx1.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).locality(l1).finish();
    auto sup2 = ctx2.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).locality(l2).finish();

    sup2->configurer = [&](auto &, r::plugin::plugin_base_t &plugin) {
        plugin.with_casted<r::plugin::link_client_plugin_t>(
            [&](auto &p) { p.link(sup1->get_address(), false, [&](auto &) {}); });
    };

    auto process_12 = [&]() {
        while (!sup1->get_leader_queue().empty() || !sup2->get_leader_queue().empty()) {
            sup1->do_process();
            sup2->do_process();
        }
    };

    process_12();

    REQUIRE(sup1->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(sup2->get_state() == r::state_t::OPERATIONAL);

    sup1->do_shutdown();
    sup1->do_process();

    sup2->do_shutdown();
    process_12();

    CHECK(sup1->get_state() == r::state_t::SHUT_DOWN);
    CHECK(sup2->get_state() == r::state_t::SHUT_DOWN);
}
