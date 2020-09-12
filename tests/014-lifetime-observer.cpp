//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "actor_test.h"
#include "supervisor_test.h"
#include "access.h"

namespace r = rotor;
namespace rt = r::test;

struct observer_config_t : r::actor_config_t {
    r::address_ptr_t observable;
    using r::actor_config_t::actor_config_t;
};

template <typename Actor> struct observer_config_builder_t : r::actor_config_builder_t<Actor> {
    using builder_t = typename Actor::template config_builder_t<Actor>;
    using parent_t = r::actor_config_builder_t<Actor>;
    using parent_t::parent_t;

    builder_t &&observable(const r::address_ptr_t &addr) {
        parent_t::config.observable = addr;
        return std::move(*static_cast<builder_t *>(this));
    }

    bool validate() noexcept override { return parent_t::config.observable && parent_t::validate(); }
};

struct observer_t : public r::actor_base_t {
    using config_t = observer_config_t;
    template <typename Actor> using config_builder_t = observer_config_builder_t<Actor>;

    explicit observer_t(config_t &cfg) : r::actor_base_t(cfg), observable{cfg.observable} {}

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        using namespace r::plugin;
        plugin.with_casted<starter_plugin_t>(
            [this](auto &p) {
                p.subscribe_actor(&observer_t::on_sample_initialize, observable);
                p.subscribe_actor(&observer_t::on_sample_start, observable);
                p.subscribe_actor(&observer_t::on_sample_shutdown, observable);
            },
            config_phase_t::PREINIT);
    }

    void on_sample_initialize(r::message::init_request_t &) noexcept { event += 1; }

    void on_sample_start(r::message_t<r::payload::start_actor_t> &) noexcept { event += 2; }

    void on_sample_shutdown(r::message::shutdown_request_t &) noexcept { event += 4; }

    std::uint32_t event = 0;
    r::address_ptr_t observable;
};

TEST_CASE("lifetime observer, same locality", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto sample_actor = sup->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto &address = sample_actor->get_address();
    auto observer = sup->create_actor<observer_t>().observable(address).timeout(rt::default_timeout).finish();

    sup->do_process();
    REQUIRE(observer->event == 3);

    sample_actor->do_shutdown();
    sup->do_process();
    REQUIRE(observer->event == 7);

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    CHECK(rt::empty(sup->get_subscription()));
}

TEST_CASE("lifetime observer, different localities", "[actor]") {
    r::system_context_t system_context;

    const char locality1[] = "l1";
    const char locality2[] = "l2";
    auto sup1 = system_context.create_supervisor<rt::supervisor_test_t>()
                    .locality(locality1)
                    .timeout(rt::default_timeout)
                    .finish();
    auto sup2 = sup1->create_actor<rt::supervisor_test_t>().locality(locality2).timeout(rt::default_timeout).finish();
    auto sample_actor = sup2->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    auto &address = sample_actor->get_address();
    auto observer = sup1->create_actor<observer_t>().observable(address).timeout(rt::default_timeout).finish();

    sup1->do_process();
    CHECK(observer->event == 0);
    CHECK(observer->access<rt::to::state>() == r::state_t::INITIALIZING);

    sup1->do_process();
    sup2->do_process();
    CHECK(sample_actor->get_state() == r::state_t::OPERATIONAL);
    CHECK(observer->access<rt::to::state>() == r::state_t::INITIALIZING);

    sup1->do_process();
    sup2->do_process();

    CHECK(observer->access<rt::to::state>() == r::state_t::OPERATIONAL);
    CHECK(observer->event == 3);

    sample_actor->do_shutdown();
    sup2->do_process();
    sup1->do_process();
    sup2->do_process();
    sup1->do_process();

    REQUIRE(observer->event == 7);

    sup1->do_shutdown();
    sup1->do_process();
    std::cout << "sup1->do_process()\n";
    sup2->do_process();
    sup1->do_process();
    sup2->do_process();

    REQUIRE(sup1->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup1->get_leader_queue().size() == 0);
    REQUIRE(sup1->get_points().size() == 0);
    CHECK(rt::empty(sup1->get_subscription()));
}
