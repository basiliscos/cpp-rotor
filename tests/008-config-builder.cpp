//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "supervisor_test.h"
#include "actor_test.h"
#include "system_context_test.h"

namespace r = rotor;
namespace rt = rotor::test;

template <typename Actor> struct sample_config_builder_t : public r::actor_config_builder_t<Actor> {
    using parent_t = r::actor_config_builder_t<Actor>;
    using parent_t::parent_t;

    bool validate() noexcept override { return false; }
};

struct sample_actor_t : public rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;
    template <typename Actor> using config_builder_t = sample_config_builder_t<Actor>;
};

TEST_CASE("direct builder configuration", "[config_builder]") {
    r::system_context_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>()
                   .timeout(rt::default_timeout)
                   .locality(&system_context)
                   .finish();
    REQUIRE(sup);
    REQUIRE(sup->locality == &system_context);
    sup->do_process();
    sup->do_shutdown();
    sup->do_process();
    sup->do_process();
    CHECK(sup->get_subscription().empty());
}

TEST_CASE("indirect builder configuration", "[config_builder]") {
    r::system_context_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>()
                   .locality(&system_context)
                   .timeout(rt::default_timeout)
                   .finish();
    REQUIRE(sup);
    REQUIRE(sup->locality == &system_context);
    sup->do_process();
    sup->do_shutdown();
    sup->do_process();
}

TEST_CASE("validation", "[config_builder]") {
    rt::system_context_test_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>()
                   .locality(&system_context)
                   .timeout(rt::default_timeout)
                   .finish();
    REQUIRE(sup);

    SECTION("custom validator failed") {
        auto act = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();
        REQUIRE(!act);
        REQUIRE(system_context.ec.value() == static_cast<int>(r::error_code_t::actor_misconfigured));
    }

    SECTION("unlink_timeout > shutdown_timeout") {
        r::pt::time_duration t1 = r::pt::milliseconds{1};
        r::pt::time_duration t2 = r::pt::milliseconds{2};
        auto act = sup->create_actor<r::actor_base_t>().timeout(t1).unlink_timeout(t2).finish();
        REQUIRE(!act);
        REQUIRE(system_context.ec.value() == static_cast<int>(r::error_code_t::actor_misconfigured));
    }

    sup->do_process();
    sup->do_shutdown();
    sup->do_process();
}
