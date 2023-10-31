//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "access.h"
#include "rotor.hpp"
#include "supervisor_test.h"
#include "actor_test.h"

namespace r = rotor;
namespace rt = r::test;

TEST_CASE("release/aquire resources, when other actor is in progress of configuration", "[plugin]") {
    r::system_context_t system_context;
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>()
                   .timeout(rt::default_timeout)
                   .create_registry(true)
                   .finish();
    auto act = sup->create_actor<rt::actor_test_t>().timeout(rt::default_timeout).finish();
    act->configurer = [&](auto &, r::plugin::plugin_base_t &plugin) {
        plugin.with_casted<r::plugin::starter_plugin_t>([&](auto &) {
            auto res = act->access<rt::to::resources>();
            res->acquire(0);
            res->release(0);
        });
    };

    sup->do_process();
    CHECK(sup->get_state() == r::state_t::OPERATIONAL);
    CHECK(act->get_state() == r::state_t::OPERATIONAL);

    sup->do_shutdown();
    sup->do_process();
    CHECK(sup->get_state() == r::state_t::SHUT_DOWN);
    CHECK(act->get_state() == r::state_t::SHUT_DOWN);
}
