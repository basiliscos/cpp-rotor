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

struct sample_sup_t;

struct sample_sup_t : public rt::supervisor_test_t {
    using sup_base_t = rt::supervisor_test_t;
    std::uint32_t initialized = 0;
#if 0
    std::uint32_t init_invoked = 0;
    std::uint32_t shutdown_started = 0;
#endif
    std::uint32_t shutdown_finished = 0;
    std::uint32_t shutdown_conf_invoked = 0;
    r::address_ptr_t shutdown_addr;

    using rt::supervisor_test_t::supervisor_test_t;

    ~sample_sup_t() override { ++destroyed; }

    void do_initialize(r::system_context_t *ctx) noexcept override {
        ++initialized;
        sup_base_t::do_initialize(ctx);
    }

#if 0
    void init_start() noexcept override {
        ++init_invoked;
        sup_base_t::init_start();
    }
#endif

    virtual void shutdown_finish() noexcept override {
        ++shutdown_finished;
        rt::supervisor_test_t::shutdown_finish();
    }

#if 0
    virtual void shutdown_start() noexcept override {
        ++shutdown_started;
        rt::supervisor_test_t::shutdown_start();
    }
#endif
};

TEST_CASE("on_initialize, on_start, simple on_shutdown", "[supervisor]") {
    r::system_context_t *system_context = new r::system_context_t{};
    auto sup = system_context->create_supervisor<sample_sup_t>().timeout(rt::default_timeout).finish();

    REQUIRE(&sup->get_supervisor() == sup.get());
    REQUIRE(sup->initialized == 1);

    sup->do_process();
#if 0
    REQUIRE(sup->init_invoked == 1);
    REQUIRE(sup->shutdown_started == 0);
#endif
    REQUIRE(sup->shutdown_conf_invoked == 0);
    REQUIRE(sup->active_timers.size() == 0);
    REQUIRE(sup->get_state() == r::state_t::OPERATIONAL);

    sup->do_shutdown();
    sup->do_process();
    //REQUIRE(sup->shutdown_started == 1);
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
