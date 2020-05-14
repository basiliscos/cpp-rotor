#include "actor_test.h"

using namespace rotor::test;
using namespace rotor;

plugin_t* actor_test_t::get_plugin(const void* identity) const noexcept {
    for(auto plugin: active_plugins){
        if (plugin->identity() == identity) { return plugin; }
    }
    for(auto plugin: inactive_plugins){
        if (plugin->identity() == identity) { return plugin; }
    }
    std::abort();
}
