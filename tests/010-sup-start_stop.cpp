//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "supervisor_test.h"
#include "actor_test.h"
#include "access.h"

namespace r = rotor;
namespace rt = rotor::test;
using namespace Catch::Matchers;

static std::uint32_t destroyed = 0;

struct init_shutdown_plugin_t;

namespace payload {
struct sample_payload_t {};
} // namespace payload

namespace message {
using sample_payload_t = r::message_t<payload::sample_payload_t>;
}

struct sample_sup_t : public rt::supervisor_test_t {
    using sup_base_t = rt::supervisor_test_t;

    using plugins_list_t = std::tuple<r::plugin::address_maker_plugin_t, r::plugin::locality_plugin_t,
                                      r::plugin::delivery_plugin_t<r::plugin::local_delivery_t>,
                                      r::plugin::lifetime_plugin_t, init_shutdown_plugin_t, /* use custom */
                                      r::plugin::foreigners_support_plugin_t, r::plugin::child_manager_plugin_t,
                                      r::plugin::starter_plugin_t>;

    std::uint32_t initialized = 0;
    std::uint32_t init_invoked = 0;
    std::uint32_t shutdown_started = 0;
    std::uint32_t shutdown_finished = 0;
    std::uint32_t shutdown_conf_invoked = 0;
    r::address_ptr_t shutdown_addr;

    using rt::supervisor_test_t::supervisor_test_t;

    ~sample_sup_t() override { ++destroyed; }

    void do_initialize(r::system_context_t *ctx) noexcept override {
        ++initialized;
        sup_base_t::do_initialize(ctx);
    }

    void shutdown_finish() noexcept override {
        ++shutdown_finished;
        rt::supervisor_test_t::shutdown_finish();
    }
};

struct init_shutdown_plugin_t : r::plugin::init_shutdown_plugin_t {
    using parent_t = r::plugin::init_shutdown_plugin_t;

    void deactivate() noexcept override { parent_t::deactivate(); }

    bool handle_shutdown(r::message::shutdown_request_t *message) noexcept override {
        auto sup = static_cast<sample_sup_t *>(actor);
        sup->shutdown_started++;
        return parent_t::handle_shutdown(message);
    }

    bool handle_init(r::message::init_request_t *message) noexcept override {
        auto sup = static_cast<sample_sup_t *>(actor);
        sup->init_invoked++;
        return parent_t::handle_init(message);
    }
};

struct sample_plugin_t : r::plugin::plugin_base_t {
    using parent_t = r::plugin::plugin_base_t;

    static const void *class_identity;

    const void *identity() const noexcept override { return class_identity; }

    void activate(r::actor_base_t *actor_) noexcept override {
        parent_t::activate(actor_);
        auto info = subscribe(&sample_plugin_t::on_message);
        info->tag_io();
        info->tag_io(); // for better coverage
    }

    void deactivate() noexcept override { parent_t::deactivate(); }

    void on_message(message::sample_payload_t &) noexcept { message_received = true; }

    bool message_received = false;
};

const void *sample_plugin_t::class_identity = &sample_plugin_t::class_identity;

struct sample_sup2_t : public rt::supervisor_test_t {
    using sup_base_t = rt::supervisor_test_t;

    std::uint32_t initialized = 0;
    std::uint32_t init_invoked = 0;
    std::uint32_t shutdown_finished = 0;
    std::uint32_t shutdown_conf_invoked = 0;
    r::address_ptr_t shutdown_addr;
    actor_base_t *init_child = nullptr;
    actor_base_t *shutdown_child = nullptr;
    r::extended_error_ptr_t init_ec;

    using rt::supervisor_test_t::supervisor_test_t;

    ~sample_sup2_t() override { ++destroyed; }

    void do_initialize(r::system_context_t *ctx) noexcept override {
        ++initialized;
        sup_base_t::do_initialize(ctx);
    }

    void init_finish() noexcept override {
        ++init_invoked;
        sup_base_t::init_finish();
    }

    virtual void shutdown_finish() noexcept override {
        ++shutdown_finished;
        rt::supervisor_test_t::shutdown_finish();
    }

    void on_child_init(actor_base_t *actor, const r::extended_error_ptr_t &ec) noexcept override {
        init_child = actor;
        init_ec = ec;
    }

    void on_child_shutdown(actor_base_t *actor) noexcept override {
        shutdown_child = actor;
    }
};

struct sample_sup3_t : public rt::supervisor_test_t {
    using sup_base_t = rt::supervisor_test_t;
    using rt::supervisor_test_t::supervisor_test_t;
    std::uint32_t received = 0;

    void make_subscription() noexcept {
        subscribe(&sample_sup3_t::on_sample);
        send<payload::sample_payload_t>(address);
    }

    void on_sample(message::sample_payload_t &) noexcept { ++received; }
};

struct sample_sup4_t : public rt::supervisor_test_t {
    using sup_base_t = rt::supervisor_test_t;
    using rt::supervisor_test_t::supervisor_test_t;
    std::uint32_t counter = 0;

    void intercept(r::message_ptr_t &, const void *tag, const r::continuation_t &continuation) noexcept override {
        CHECK(tag == rotor::tags::io);
        if (++counter % 2) {
            continuation();
        }
    }
};

struct unsubscriber_sup_t : public rt::supervisor_test_t {
    using sup_base_t = rt::supervisor_test_t;
    using rt::supervisor_test_t::supervisor_test_t;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<r::plugin::starter_plugin_t>(
            [](auto &p) { p.subscribe_actor(&unsubscriber_sup_t::on_sample); });
    }

    void on_start() noexcept override {
        rt::supervisor_test_t::on_start();
        unsubscribe(&unsubscriber_sup_t::on_sample);
    }

    void on_sample(message::sample_payload_t &) noexcept {}
};

struct sample_actor_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;
};

struct sample_actor2_t : public rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<r::plugin::address_maker_plugin_t>([&](auto &p) {
            alternative = p.create_address();
            p.set_identity("specific_name", false);
        });
        plugin.with_casted<r::plugin::starter_plugin_t>([&](auto &p) {
            p.subscribe_actor(&sample_actor2_t::on_link, alternative);
            send<payload::sample_payload_t>(alternative);
        });
    }

    void on_link(message::sample_payload_t &) noexcept { ++received; }

    r::address_ptr_t alternative;
    int received = 0;
};

struct sample_actor3_t : public rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;

    void shutdown_start() noexcept override {
        rt::actor_test_t::shutdown_start();
        resources->acquire();
    }
};

struct sample_actor4_t : public rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        rt::actor_test_t::configure(plugin);
        plugin.with_casted<r::plugin::starter_plugin_t>(
            [&](auto &p) { p.subscribe_actor(&sample_actor4_t::on_message)->tag_io(); });
    }

    void on_start() noexcept override {
        rt::actor_test_t::on_start();
        send<payload::sample_payload_t>(get_address());
        send<payload::sample_payload_t>(get_address());
    }

    void on_message(message::sample_payload_t &) noexcept { ++received; }
    std::size_t received = 0;
};

struct sample_actor5_t : public rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;

    // clang-format off
    using plugins_list_t = std::tuple<
        r::plugin::address_maker_plugin_t,
        r::plugin::lifetime_plugin_t,
        r::plugin::init_shutdown_plugin_t,
        r::plugin::link_server_plugin_t,
        r::plugin::link_client_plugin_t,
        r::plugin::registry_plugin_t,
        r::plugin::resources_plugin_t,
        r::plugin::starter_plugin_t,
        sample_plugin_t
    >;
    // clang-format on

    void on_start() noexcept override {
        rt::actor_test_t::on_start();
        send<payload::sample_payload_t>(get_address());
        send<payload::sample_payload_t>(get_address());
    }
};

struct sample_actor6_t : public rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;

    void on_start() noexcept override {
        rt::actor_test_t::on_start();
        start_timer(r::pt::minutes(1), *this, &sample_actor6_t::on_timer);
    }

    void on_timer(r::request_id_t, bool cancelled) noexcept { this->cancelled = cancelled; }
    bool cancelled = false;
};

struct sample_actor7_t : public rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;
    r::message_ptr_t msg;

    void on_start() noexcept override {
        rt::actor_test_t::on_start();
        subscribe(&sample_actor7_t::on_message);
        do_shutdown();
    }

    void shutdown_start() noexcept override {
        auto sup = static_cast<rt::supervisor_test_t *>(supervisor);
        sup->get_leader_queue().push_back(std::move(msg));
        rt::actor_test_t::shutdown_start();
    }

    void on_message(message::sample_payload_t &) noexcept {}
};

TEST_CASE("on_initialize, on_start, simple on_shutdown (handled by plugin)", "[supervisor]") {
    destroyed = 0;
    r::system_context_t *system_context = new r::system_context_t{};
    auto sup = system_context->create_supervisor<sample_sup_t>().timeout(rt::default_timeout).finish();

    REQUIRE(&sup->get_supervisor() == sup.get());
    REQUIRE(sup->initialized == 1);
    auto& identity = sup->get_identity();
    CHECK_THAT(identity, StartsWith("supervisor"));

    sup->do_process();
    CHECK(sup->init_invoked == 1);
    CHECK(sup->shutdown_started == 0);
    CHECK(sup->shutdown_conf_invoked == 0);
    CHECK(sup->active_timers.size() == 0);
    CHECK(sup->get_state() == r::state_t::OPERATIONAL);

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->shutdown_started == 1);
    REQUIRE(sup->shutdown_finished == 1);
    REQUIRE(sup->active_timers.size() == 0);

    REQUIRE(sup->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    CHECK(rt::empty(sup->get_subscription()));

    REQUIRE(destroyed == 0);
    delete system_context;

    sup->shutdown_addr.reset();
    sup.reset();
    REQUIRE(destroyed == 1);
}

TEST_CASE("on_initialize, on_start, simple on_shutdown", "[supervisor]") {
    destroyed = 0;
    r::system_context_t *system_context = new r::system_context_t{};
    auto sup = system_context->create_supervisor<sample_sup2_t>().timeout(rt::default_timeout).finish();

    REQUIRE(&sup->get_supervisor() == sup.get());
    REQUIRE(sup->initialized == 1);
    REQUIRE(sup->init_child == nullptr);

    sup->do_process();
    REQUIRE(sup->init_invoked == 1);
    REQUIRE(sup->shutdown_conf_invoked == 0);
    REQUIRE(sup->active_timers.size() == 0);
    REQUIRE(sup->get_state() == r::state_t::OPERATIONAL);

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->shutdown_finished == 1);
    REQUIRE(sup->active_timers.size() == 0);

    REQUIRE(sup->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    CHECK(rt::empty(sup->get_subscription()));
    REQUIRE(sup->shutdown_child == nullptr);

    REQUIRE(destroyed == 0);
    delete system_context;

    sup->shutdown_addr.reset();
    sup.reset();
    REQUIRE(destroyed == 1);
}

TEST_CASE("start/shutdown 1 child & 1 supervisor", "[supervisor]") {
    r::system_context_ptr_t system_context = new r::system_context_t();
    auto sup = system_context->create_supervisor<sample_sup2_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();

    CHECK_THAT(act->get_identity(), StartsWith("actor"));

    /* for better coverage */
    auto last = sup->access<rt::to::last_req_id>();
    auto &request_map = sup->access<rt::to::request_map>();
    request_map[last + 1] = r::request_curry_t();

    sup->do_process();
    request_map.clear();

    CHECK(sup->access<rt::to::last_req_id>() > 1);
    CHECK(sup->get_state() == r::state_t::OPERATIONAL);
    CHECK(act->access<rt::to::state>() == r::state_t::OPERATIONAL);
    CHECK(act->access<rt::to::resources>()->has() == 0);
    CHECK(sup->init_child == act.get());
    CHECK(!sup->init_ec);
    CHECK(sup->shutdown_child == nullptr);

    sup->do_shutdown();
    sup->do_process();
    CHECK(sup->get_state() == r::state_t::SHUT_DOWN);
    CHECK(act->access<rt::to::state>() == r::state_t::SHUT_DOWN);
    CHECK(sup->shutdown_child == act.get());

    auto& reason = sup->shutdown_child->get_shutdown_reason();
    REQUIRE(reason);
    CHECK(reason->ec.value() == static_cast<int>(r::shutdown_code_t::supervisor_shutdown));
    auto& root = reason->next;
    CHECK(root);
    CHECK(root->ec.value() == static_cast<int>(r::shutdown_code_t::normal));
    CHECK(!root->next);
}

TEST_CASE("custom subscription", "[supervisor]") {
    r::system_context_ptr_t system_context = new r::system_context_t();
    auto sup = system_context->create_supervisor<sample_sup3_t>().timeout(rt::default_timeout).finish();
    sup->do_process();
    CHECK(sup->get_state() == r::state_t::OPERATIONAL);

    sup->make_subscription();
    sup->do_process();
    CHECK(sup->received == 1);

    sup->do_shutdown();
    sup->do_process();
    CHECK(sup->get_state() == r::state_t::SHUT_DOWN);
}

TEST_CASE("shutdown immediately", "[supervisor]") {
    r::system_context_ptr_t system_context = new r::system_context_t();
    auto sup = system_context->create_supervisor<sample_sup3_t>().timeout(rt::default_timeout).finish();
    sup->do_shutdown();
    sup->do_process();
    CHECK(sup->get_state() == r::state_t::SHUT_DOWN);
}

TEST_CASE("self unsubscriber", "[actor]") {
    r::system_context_ptr_t system_context = new r::system_context_t();
    auto sup = system_context->create_supervisor<unsubscriber_sup_t>().timeout(rt::default_timeout).finish();
    sup->do_process();
    CHECK(sup->get_state() == r::state_t::OPERATIONAL);

    sup->do_shutdown();
    sup->do_process();
    CHECK(sup->get_state() == r::state_t::SHUT_DOWN);
}

TEST_CASE("alternative address subscriber", "[actor]") {
    r::system_context_ptr_t system_context = new r::system_context_t();
    auto sup = system_context->create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<sample_actor2_t>().timeout(rt::default_timeout).finish();

    CHECK(act->get_identity() == "specific_name");

    sup->do_process();
    CHECK(sup->get_state() == r::state_t::OPERATIONAL);
    CHECK(act->get_state() == r::state_t::OPERATIONAL);
    CHECK(act->received == 1);

    sup->do_shutdown();
    sup->do_process();
    CHECK(sup->get_state() == r::state_t::SHUT_DOWN);
    CHECK(act->get_state() == r::state_t::SHUT_DOWN);
}

TEST_CASE("acquire resources on shutdown start", "[actor]") {
    r::system_context_ptr_t system_context = new r::system_context_t();
    auto sup = system_context->create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<sample_actor3_t>().timeout(rt::default_timeout).finish();
    sup->do_process();
    CHECK(sup->get_state() == r::state_t::OPERATIONAL);

    sup->do_shutdown();
    sup->do_process();
    CHECK(act->get_state() == r::state_t::SHUTTING_DOWN);

    act->access<rt::to::resources>()->release();
    sup->do_process();
    CHECK(sup->get_state() == r::state_t::SHUT_DOWN);
    CHECK(act->get_state() == r::state_t::SHUT_DOWN);
}

TEST_CASE("io tagging & intercepting", "[actor]") {
    r::system_context_ptr_t system_context = new r::system_context_t();
    auto sup = system_context->create_supervisor<sample_sup4_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<sample_actor4_t>().timeout(rt::default_timeout).finish();
    sup->do_process();
    CHECK(sup->get_state() == r::state_t::OPERATIONAL);

    CHECK(act->received == 1);
    CHECK(sup->counter == 2);

    sup->do_shutdown();
    sup->do_process();

    CHECK(act->get_state() == r::state_t::SHUT_DOWN);
    CHECK(sup->get_state() == r::state_t::SHUT_DOWN);
}

TEST_CASE("io tagging (in plugin) & intercepting", "[actor]") {
    r::system_context_ptr_t system_context = new r::system_context_t();
    auto sup = system_context->create_supervisor<sample_sup4_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<sample_actor5_t>().timeout(rt::default_timeout).finish();
    sup->do_process();
    CHECK(sup->get_state() == r::state_t::OPERATIONAL);

    CHECK(sup->counter == 2);
    auto plugin = act->access<rt::to::get_plugin>(sample_plugin_t::class_identity);
    CHECK(plugin);
    CHECK(static_cast<sample_plugin_t *>(plugin)->message_received);

    sup->do_shutdown();
    sup->do_process();

    CHECK(act->get_state() == r::state_t::SHUT_DOWN);
    CHECK(sup->get_state() == r::state_t::SHUT_DOWN);
}

TEST_CASE("timers cancellation", "[actor]") {
    r::system_context_ptr_t system_context = new r::system_context_t();
    auto sup = system_context->create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<sample_actor6_t>().timeout(rt::default_timeout).finish();
    sup->do_process();
    CHECK(act->get_state() == r::state_t::OPERATIONAL);
    CHECK(sup->get_state() == r::state_t::OPERATIONAL);
    CHECK(!act->access<rt::to::timers_map>().empty());

    sup->do_shutdown();
    sup->do_process();

    CHECK(act->get_state() == r::state_t::SHUT_DOWN);
    CHECK(sup->get_state() == r::state_t::SHUT_DOWN);
    CHECK(act->access<rt::to::timers_map>().empty());
}

TEST_CASE("subscription confirmation arrives on non-init phase", "[actor]") {
    r::system_context_ptr_t system_context = new r::system_context_t();
    auto sup = system_context->create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<sample_actor7_t>().timeout(rt::default_timeout).finish();

    auto act_configurer = [&](auto &, r::plugin::plugin_base_t &plugin) {
        plugin.with_casted<r::plugin::starter_plugin_t>([&](auto &p) {
            p.subscribe_actor(r::lambda<message::sample_payload_t>([](message::sample_payload_t &) noexcept { ; }));
            auto req = sup->get_leader_queue().back();
            sup->get_leader_queue().pop_back();
            act->msg = std::move(req);
            act->do_shutdown();
        });
    };
    act->configurer = act_configurer;

    sup->do_process();
    CHECK(act->get_state() == r::state_t::SHUT_DOWN);
    CHECK(sup->get_state() == r::state_t::SHUT_DOWN);
}
