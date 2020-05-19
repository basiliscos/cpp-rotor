#include "actor_test.h"

using namespace rotor::test;
using namespace rotor;

plugin_t* actor_test_t::get_plugin(const void* identity) const noexcept {
    return get_plugin(plugins, identity);
}

plugin_t* actor_test_t::get_plugin(const actor_config_t::plugins_t& plugins, const void* identity) noexcept {
    for(auto plugin: plugins){
        if (plugin->identity() == identity) { return plugin; }
    }
    std::abort();
}
