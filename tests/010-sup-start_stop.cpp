//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "supervisor_test.h"

namespace r = rotor;
namespace rt = rotor::test;

static std::uint32_t destroyed = 0;

struct init_shutdown_plugin_t;

template <typename Actor> struct sample_config_builder_t : public r::supervisor_config_builder_t<Actor> {
    using parent_t = r::supervisor_config_builder_t<Actor>;
    using parent_t::parent_t;

    using plugins_list_t = std::tuple<
        r::internal::locality_plugin_t,
        r::internal::actor_lifetime_plugin_t,
        r::internal::subscription_plugin_t,
        init_shutdown_plugin_t,                 /* use custom */
        r::internal::starter_plugin_t,
        r::internal::subscription_support_plugin_t,
        r::internal::children_manager_plugin_t
    >;
};


struct sample_sup_t : public rt::supervisor_test_t {
    using sup_base_t = rt::supervisor_test_t;

    template <typename Supervisor> using config_builder_t = sample_config_builder_t<Supervisor>;

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

    virtual void shutdown_finish() noexcept override {
        ++shutdown_finished;
        rt::supervisor_test_t::shutdown_finish();
    }

};

struct init_shutdown_plugin_t: r::internal::init_shutdown_plugin_t {
    using parent_t = r::internal::init_shutdown_plugin_t;

    virtual void confirm_init() noexcept {
        auto sup = static_cast<sample_sup_t*>(actor);
        sup->init_invoked++;
        parent_t::confirm_init();
    }

    virtual void confirm_shutdown() noexcept {
        auto sup = static_cast<sample_sup_t*>(actor);
        sup->shutdown_started++;
        parent_t::confirm_shutdown();
    }
};


struct sample_sup2_t : public rt::supervisor_test_t {
    using sup_base_t = rt::supervisor_test_t;

    std::uint32_t initialized = 0;
    std::uint32_t init_invoked = 0;
    std::uint32_t shutdown_started = 0;
    std::uint32_t shutdown_finished = 0;
    std::uint32_t shutdown_conf_invoked = 0;
    r::address_ptr_t shutdown_addr;

    using rt::supervisor_test_t::supervisor_test_t;

    ~sample_sup2_t() override { ++destroyed; }

    void do_initialize(r::system_context_t *ctx) noexcept override {
        ++initialized;
        sup_base_t::do_initialize(ctx);
    }

    void init_finish() noexcept override {
        ++init_invoked;
    }

    virtual void shutdown_start() noexcept override {
        ++shutdown_started;
        rt::supervisor_test_t::shutdown_start();
    }

    virtual void shutdown_finish() noexcept override {
        ++shutdown_finished;
        rt::supervisor_test_t::shutdown_finish();
    }
};

struct sample_actor_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;
};

#if 0
TEST_CASE("on_initialize, on_start, simple on_shutdown (handled by plugin)", "[supervisor]") {
    destroyed  = 0;
    r::system_context_t *system_context = new r::system_context_t{};
    auto sup = system_context->create_supervisor<sample_sup_t>().timeout(rt::default_timeout).finish();

    REQUIRE(&sup->get_supervisor() == sup.get());
    REQUIRE(sup->initialized == 1);

    sup->do_process();
    REQUIRE(sup->init_invoked == 1);
    REQUIRE(sup->shutdown_started == 0);
    REQUIRE(sup->shutdown_conf_invoked == 0);
    REQUIRE(sup->active_timers.size() == 0);
    REQUIRE(sup->get_state() == r::state_t::OPERATIONAL);

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->shutdown_started == 1);
    REQUIRE(sup->shutdown_finished == 1);
    REQUIRE(sup->active_timers.size() == 0);

    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().size() == 0);

    REQUIRE(destroyed == 0);
    delete system_context;

    sup->shutdown_addr.reset();
    sup.reset();
    REQUIRE(destroyed == 1);
}

TEST_CASE("on_initialize, on_start, simple on_shutdown", "[supervisor]") {
    destroyed  = 0;
    r::system_context_t *system_context = new r::system_context_t{};
    auto sup = system_context->create_supervisor<sample_sup2_t>().timeout(rt::default_timeout).finish();

    REQUIRE(&sup->get_supervisor() == sup.get());
    REQUIRE(sup->initialized == 1);

    sup->do_process();
    REQUIRE(sup->init_invoked == 1);
    REQUIRE(sup->shutdown_started == 0);
    REQUIRE(sup->shutdown_conf_invoked == 0);
    REQUIRE(sup->active_timers.size() == 0);
    REQUIRE(sup->get_state() == r::state_t::OPERATIONAL);

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->shutdown_started == 1);
    REQUIRE(sup->shutdown_finished == 1);
    REQUIRE(sup->active_timers.size() == 0);

    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().size() == 0);

    REQUIRE(destroyed == 0);
    delete system_context;

    sup->shutdown_addr.reset();
    sup.reset();
    REQUIRE(destroyed == 1);
}
#endif

TEST_CASE("start/shutdown 1 child & 1 supervisor", "[supervisor]") {
    r::system_context_ptr_t system_context = new r::system_context_t();
    auto sup = system_context->create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();
    sup->do_process();

    CHECK(sup->get_state() == r::state_t::OPERATIONAL);
    CHECK(act->get_state() == r::state_t::OPERATIONAL);

    sup->do_shutdown();
    sup->do_process();
    CHECK(sup->get_state() == r::state_t::SHUTTED_DOWN);
    CHECK(act->get_state() == r::state_t::SHUTTED_DOWN);
}
