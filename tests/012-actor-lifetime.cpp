//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "supervisor_test.h"
#include "actor_test.h"

namespace r = rotor;
namespace rt = r::test;

static std::uint32_t destroyed = 0;

struct sample_actor_t : public r::actor_base_t {
    std::uint32_t event_current;
    std::uint32_t event_init_start;
    std::uint32_t event_init_finish;
    std::uint32_t event_start;
    std::uint32_t event_shutdown_start;
    std::uint32_t event_shutdown_finish;

    r::state_t &get_state() noexcept { return state; }

    sample_actor_t(r::actor_config_t &config_) : r::actor_base_t{config_} {
        event_current = 1;
        event_init_start = 0;
        event_init_finish = 0;
        event_start = 0;
        event_shutdown_start = 0;
        event_shutdown_finish = 0;
    }
    ~sample_actor_t() override { ++destroyed; }

    void init_start() noexcept override {
        event_init_start = event_current++;
        r::actor_base_t::init_start();
    }

    void init_finish() noexcept override {
        event_init_finish = event_current++;
        r::actor_base_t::init_finish();
    }

    void on_start() noexcept override{
        event_start = event_current++;
        r::actor_base_t::on_start();
    }


    void shutdown_start() noexcept override {
        event_shutdown_start = event_current++;
        r::actor_base_t::shutdown_start();
    }

    void shutdown_finish() noexcept override {
        event_shutdown_finish = event_current++;
        r::actor_base_t::shutdown_finish();
    }
};

struct custom_child_manager_t: public r::internal::children_manager_plugin_t {
    r::address_ptr_t fail_addr;
    std::error_code fail_ec;
    void on_shutdown_fail(r::actor_base_t &actor, const std::error_code &ec) noexcept {
        fail_addr = actor.get_address();
        fail_ec = ec;
    }
};

template <typename Actor> struct custom_sup_config_builder_t : public rt::supervisor_test_config_builder_t<Actor> {
    using parent_t = rt::supervisor_test_config_builder_t<Actor>;
    using parent_t::parent_t;

    using plugins_list_t = std::tuple<
        r::internal::locality_plugin_t,
        r::internal::actor_lifetime_plugin_t,
        r::internal::subscription_plugin_t,
        r::internal::init_shutdown_plugin_t,
        r::internal::initializer_plugin_t,
        r::internal::starter_plugin_t,
        r::internal::subscription_support_plugin_t,
        custom_child_manager_t
    >;
};

struct custom_supervisor_t: rt::supervisor_test_t {
    using rt::supervisor_test_t::supervisor_test_t;
    template <typename Supervisor> using config_builder_t = custom_sup_config_builder_t<Supervisor>;
};

#if 0
struct fail_init_plugin_t: public r::plugin_t {
    bool activate(r::actor_base_t* actor_) noexcept override {
        actor_->install_plugin(*this, r::slot_t::INIT);
        return r::plugin_t::activate(actor_);
    }

    bool handle_init(r::message::init_request_t* ) noexcept override {
        return false;
    }
};

template <typename Actor> struct fail_init_config_builder_t : public r::actor_config_builder_t<Actor> {
    using parent_t = r::actor_config_builder_t<Actor>;
    using parent_t::parent_t;

    using plugins_list_t = std::tuple<
        r::internal::actor_lifetime_plugin_t,
        r::internal::subscription_plugin_t,
        r::internal::init_shutdown_plugin_t,
        r::internal::initializer_plugin_t,
        r::internal::starter_plugin_t,
        fail_init_plugin_t
    >;
};

struct fail_init_actor_t: public r::actor_base_t {
    using r::actor_base_t::actor_base_t;
    template <typename Actor> using config_builder_t = fail_init_config_builder_t<Actor>;
};
#endif

struct fail_shutdown_plugin_t: public r::plugin_t {
    bool allow_shutdown = false;
    bool activate(r::actor_base_t* actor_) noexcept override {
        actor_->install_plugin(*this, r::slot_t::SHUTDOWN);
        return r::plugin_t::activate(actor_);
    }

    bool handle_shutdown(r::message::shutdown_request_t* ) noexcept override {
        return allow_shutdown;
    }
};

template <typename Actor> struct fail_shutdown_config_builder_t : public r::actor_config_builder_t<Actor> {
    using parent_t = r::actor_config_builder_t<Actor>;
    using parent_t::parent_t;

    using plugins_list_t = std::tuple<
        r::internal::actor_lifetime_plugin_t,
        r::internal::subscription_plugin_t,
        r::internal::init_shutdown_plugin_t,
        r::internal::initializer_plugin_t,
        r::internal::starter_plugin_t,
        fail_shutdown_plugin_t
    >;
};

struct fail_shutdown_actor_t: public rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;
    template <typename Actor> using config_builder_t = fail_shutdown_config_builder_t<Actor>;
};

#if 0
struct fail_shutdown_actor : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;

    bool allow_shutdown = false;

    void shutdown_start() noexcept override {
        if (allow_shutdown) {
            r::actor_base_t::shutdown_start();
        }
    }
};

struct double_shutdown_actor : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;

    std::uint32_t shutdown_starts = 0;

    void shutdown_start() noexcept override { ++shutdown_starts; }

    void continue_shutdown() noexcept { r::actor_base_t::shutdown_start(); }
};

struct fail_initialize_actor : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;

    void init_start() noexcept override {}
};

struct fail_test_behavior_t : public r::supervisor_behavior_t {
    using r::supervisor_behavior_t::supervisor_behavior_t;

    void on_shutdown_fail(const r::address_ptr_t &address, const std::error_code &ec) noexcept override;
};

struct fail_shutdown_sup : public rt::supervisor_test_t {
    using rt::supervisor_test_t::supervisor_test_t;

    r::address_ptr_t fail_addr;
    std::error_code fail_reason;

    virtual r::actor_behavior_t *create_behavior() noexcept override { return new fail_test_behavior_t(*this); }
};

void fail_test_behavior_t::on_shutdown_fail(const r::address_ptr_t &address, const std::error_code &ec) noexcept {
    auto &sup = static_cast<fail_shutdown_sup &>(actor);
    sup.fail_addr = address;
    sup.fail_reason = ec;
}
#endif

TEST_CASE("actor litetimes", "[actor]") {
    r::system_context_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();

    REQUIRE(act->get_state() == r::state_t::INITIALIZING);

    sup->do_process();
    REQUIRE(act->get_state() == r::state_t::OPERATIONAL);

    auto actor_addr = act->get_address();
    act->do_shutdown();
    sup->do_process();
    CHECK(act->event_current == 6);
    CHECK(act->event_shutdown_finish == 5);
    CHECK(act->event_shutdown_start == 4);
    CHECK(act->event_start == 3);
    CHECK(act->event_init_finish == 2);
    CHECK(act->event_init_start == 1);

    REQUIRE(destroyed == 0);
    REQUIRE(act->get_state() == r::state_t::SHUTTED_DOWN);
    act.reset();
    REQUIRE(destroyed == 1);

    /* for asan */
    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().size() == 0);
}

TEST_CASE("fail shutdown test", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<custom_supervisor_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<fail_shutdown_actor_t>().timeout(rt::default_timeout).finish();

    sup->do_process();
    REQUIRE(sup->active_timers.size() == 0);

    act->do_shutdown();
    sup->do_process();
    REQUIRE(sup->active_timers.size() == 1); // "init child + shutdown children"

    sup->on_timer_trigger(*sup->active_timers.begin());
    sup->do_process();

    REQUIRE(sup->get_children().size() == 1);
    CHECK(act->get_state() == r::state_t::SHUTTING_DOWN);

    auto cm_plugin = static_cast<custom_child_manager_t*>(sup->get_inactive_plugins().front());

    REQUIRE(cm_plugin->fail_addr == act->get_address());
    REQUIRE(cm_plugin->fail_ec.value() == static_cast<int>(r::error_code_t::request_timeout));

    auto fail_plugin = static_cast<fail_shutdown_plugin_t*>(act->get_inactive_plugins().front());
    fail_plugin->allow_shutdown = true;
    act->shutdown_continue();

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->get_children().size() == 0);

    CHECK(act->get_state() == r::state_t::SHUTTED_DOWN);
    CHECK(sup->get_state() == r::state_t::SHUTTED_DOWN);
    CHECK(sup->get_leader_queue().size() == 0);
    CHECK(sup->get_points().size() == 0);
    CHECK(sup->get_subscription().size() == 0);

}
#if 0

TEST_CASE("fail initialize test", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    sup->do_process();

    auto act = sup->create_actor<fail_initialize_actor>().timeout(rt::default_timeout).finish();
    sup->do_process();

    REQUIRE(sup->get_children().size() == 1);
    REQUIRE(act->get_state() == r::state_t::INITIALIZING);
    REQUIRE(sup->active_timers.size() == 1);

    sup->on_timer_trigger(*sup->active_timers.begin());
    sup->do_process();
    REQUIRE(sup->get_children().size() == 0);

    sup->do_shutdown();
    sup->do_process();
}

TEST_CASE("double shutdown test (actor)", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<double_shutdown_actor>().timeout(rt::default_timeout).finish();

    sup->do_process();

    act->do_shutdown();
    act->do_shutdown();
    sup->do_process();
    REQUIRE(sup->active_timers.size() == 1);
    REQUIRE(act->shutdown_starts == 1);

    act->continue_shutdown();
    sup->do_process();

    REQUIRE(sup->get_children().size() == 0);

    sup->do_shutdown();
    sup->do_process();
}

TEST_CASE("double shutdown test (supervisor)", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto act = sup->create_actor<double_shutdown_actor>().timeout(rt::default_timeout).finish();

    sup->do_process();

    sup->do_shutdown();
    sup->do_shutdown();

    sup->do_process();
    REQUIRE(sup->active_timers.size() == 1);
    REQUIRE(act->shutdown_starts == 1);

    act->continue_shutdown();
    sup->do_process();

    REQUIRE(sup->get_children().size() == 0);

    sup->do_shutdown();
    sup->do_process();
}
#endif
