//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "actor_test.h"
#include "access.h"

using namespace rotor::test;
using namespace rotor;

actor_test_t::~actor_test_t() { printf("~actor_test_t, %p(%p)\n", (void *)this, (void *)address.get()); }

void actor_test_t::configure(plugin::plugin_base_t &plugin) noexcept {
    actor_base_t::configure(plugin);
    if (configurer)
        configurer(*this, plugin);
}

void actor_test_t::force_cleanup() noexcept {
    for (auto plugin : plugins) {
        plugin->access<to::own_subscriptions>().clear();
    }
    lifetime->access<to::points>().clear();
}

void actor_test_t::shutdown_finish() noexcept {
    actor_base_t::shutdown_finish();
    if (shutdowner)
        shutdowner(*this);
}
