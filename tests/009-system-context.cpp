//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "supervisor_test.h"
#include "system_context_test.h"

namespace r = rotor;
namespace rt = rotor::test;

TEST_CASE("misconfigured root supervisor", "[system_context]") {
    rt::system_context_test_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().finish();
    REQUIRE(!sup);
    REQUIRE(system_context.ec.value() == static_cast<int>(r::error_code_t::actor_misconfigured));
    REQUIRE(!system_context.get_supervisor());
}


TEST_CASE("properly configured root supervisor", "[system_context]") {
    rt::system_context_test_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    REQUIRE(sup);
    REQUIRE(system_context.ec.value() == 0);
    REQUIRE(system_context.get_supervisor() == sup);

    //sup->do_process();
    sup->do_shutdown();
    sup->do_process();
    sup.reset();
}
