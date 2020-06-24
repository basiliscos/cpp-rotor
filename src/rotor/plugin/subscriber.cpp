#include "rotor/plugin/subscriber.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::internal;

void subscriber_plugin_t::deactivate() noexcept {
    tracked.clear();
    return plugin_t::deactivate();
}
