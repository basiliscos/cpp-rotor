//
// Copyright (c) 2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor.hpp"
#include <catch2/catch_test_macros.hpp>

namespace r = rotor;

TEST_CASE("error code messages", "[misc]") {
    CHECK(r::error_code_category().name() == std::string("rotor_error"));
    CHECK(r::error_code_category().message(-1) == "unknown");

    CHECK(r::shutdown_code_category().name() == std::string("rotor_shutdown"));
    CHECK(r::shutdown_code_category().message(-1) == "unknown shutdown reason");
}
