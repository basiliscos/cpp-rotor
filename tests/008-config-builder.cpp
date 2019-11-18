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

TEST_CASE("direct builder configuration", "[config_builder]") {
    r::system_context_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>()
            .timeout(rt::default_timeout)
            .locality(&system_context)
            .finish();
    REQUIRE(sup);
    REQUIRE(sup->locality == &system_context);
    sup->do_shutdown();
    sup->do_process();
}

TEST_CASE("indirect builder configuration", "[config_builder]") {
    r::system_context_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>()
            .locality(&system_context)
            .timeout(rt::default_timeout)
            .finish();
    REQUIRE(sup);
    REQUIRE(sup->locality == &system_context);
    sup->do_shutdown();
    sup->do_process();
}
