//
// Copyright (c) 2024 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor.hpp"
#include "rotor/extended_error.h"
#include "supervisor_test.h"

#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace Catch::Matchers;

namespace r = rotor;
namespace rt = rotor::test;

namespace payload {

struct ping_t {};

} // namespace payload

namespace message {

using ping_t = r::message_t<payload::ping_t>;

}

TEST_CASE("default message stringifier", "[misc]") {
    r::system_context_ptr_t system_context = new r::system_context_t();
    auto sup = system_context->create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto &stringifer = system_context->get_stringifier();

    SECTION("rotor message") {
        auto start_message = r::make_message<r::payload::start_actor_t>(sup->get_address());
        auto message = stringifer.stringify(*start_message);
        CHECK_THAT(message, StartsWith("r::start_trigger @"));
    }

    SECTION("unknown / non-rotor message") {
        auto ping = r::make_message<payload::ping_t>(sup->get_address());
        auto message = stringifer.stringify(*ping);
        CHECK_THAT(message, ContainsSubstring("[?]"));
        CHECK_THAT(message, ContainsSubstring("rotor::message_t"));
        CHECK_THAT(message, ContainsSubstring("payload::ping_t"));
        CHECK_THAT(message, ContainsSubstring(" =>"));
    }

    SECTION("extended error") {
        auto ping = r::make_message<r::payload::start_actor_t>(sup->get_address());
        auto context = std::string("some-actor");
        auto ee = r::make_error(context, {}, {}, ping);
        auto string = ee->message(&stringifer);
        CHECK_THAT(string, ContainsSubstring("[r::start_trigger @"));
        CHECK_THAT(string, ContainsSubstring("some-actor"));
    }

    sup->do_process();
    sup->do_shutdown();
    sup->do_process();
}
