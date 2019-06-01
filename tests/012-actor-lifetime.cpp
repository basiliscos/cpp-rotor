#include "catch.hpp"
#include "rotor.hpp"
#include "supervisor_test.h"

namespace r = rotor;
namespace rt = r::test;

struct sample_actor_t : public r::actor_base_t {
    std::uint32_t event_current;
    std::uint32_t event_init;
    std::uint32_t event_start;
    std::uint32_t event_shutdown;

    sample_actor_t(r::supervisor_t &sup): r::actor_base_t{sup} {
        event_current = 1;
        event_init = 0;
        event_start = 0;
        event_shutdown = 0;
    }

    void on_initialize(r::message_t<r::payload::initialize_actor_t> &msg) noexcept override {
        event_init = event_current++;
        r::actor_base_t::on_initialize(msg);
    }

    void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        event_start = event_current++;
        r::actor_base_t::on_start(msg);
    }

    void on_shutdown(r::message_t<r::payload::shutdown_request_t> &msg) noexcept override {
        event_shutdown = event_current++;
        r::actor_base_t::on_shutdown(msg);
    }
};


TEST_CASE("actor litetimes", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>();
    auto act = sup->create_actor<sample_actor_t>();

    sup->do_start();
    sup->do_process();
    act->do_shutdown();
    sup->do_process();

    REQUIRE(act->event_current == 4);
    REQUIRE(act->event_shutdown == 3);
    REQUIRE(act->event_start == 2);
    REQUIRE(act->event_init == 1);
}
