#include "rotor/plugin/subscriber.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::plugin;

void subscriber_plugin_t::deactivate() noexcept {
    tracked.clear();
    return plugin_base_t::deactivate();
}
