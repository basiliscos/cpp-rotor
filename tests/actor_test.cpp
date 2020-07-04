#include "actor_test.h"

using namespace rotor::test;
using namespace rotor;

actor_test_t::~actor_test_t(){
    printf("~actor_test_t, %p(%p)\n", (void*)this, (void*)address.get());
}

plugin_t* actor_test_t::get_plugin(const void* identity) const noexcept {
    return get_plugin(plugins, identity);
}

plugin_t* actor_test_t::get_plugin(const actor_config_t::plugins_t& plugins, const void* identity) noexcept {
    for(auto plugin: plugins){
        if (plugin->identity() == identity) { return plugin; }
    }
    std::abort();
}

void actor_test_t::configure(plugin_t &plugin) noexcept {
    actor_base_t::configure(plugin);
    if (configurer) configurer(*this, plugin);
}


void actor_test_t::force_cleanup() noexcept {
    for(auto plugin: plugins ) {
       plugin->own_subscriptions.clear();
    }
    lifetime->points.clear();
}
