#include "actor_test.h"

using namespace rotor::test;
using namespace rotor;

actor_test_t::~actor_test_t(){
    printf("~actor_test_t, %p(%p)\n", (void*)this, (void*)address.get());
}

void actor_test_t::configure(plugin_t &plugin) noexcept {
    actor_base_t::configure(plugin);
    if (configurer) configurer(*this, plugin);
}


void actor_test_t::force_cleanup() noexcept {
    for(auto plugin: plugins ) {
       plugin->get_subscriptions().clear();
    }
    lifetime->get_points().clear();
}
