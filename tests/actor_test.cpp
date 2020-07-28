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
