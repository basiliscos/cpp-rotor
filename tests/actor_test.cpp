#include "actor_test.h"

using namespace rotor::test;
using namespace rotor;

template<> auto& plugin_t::access<actor_test_t>() noexcept { return own_subscriptions; }
template<> auto& internal::lifetime_plugin_t::access<actor_test_t>() noexcept { return points; }

actor_test_t::~actor_test_t(){
    printf("~actor_test_t, %p(%p)\n", (void*)this, (void*)address.get());
}

void actor_test_t::configure(plugin_t &plugin) noexcept {
    actor_base_t::configure(plugin);
    if (configurer) configurer(*this, plugin);
}


void actor_test_t::force_cleanup() noexcept {
    for(auto plugin: plugins ) {
        plugin->access<actor_test_t>().clear();
    }
    lifetime->access<actor_test_t>().clear();
}
