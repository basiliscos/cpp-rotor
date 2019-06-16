#pragma once

#include <chrono>
#include <wx/event.h>

namespace rotor {
namespace wx {

struct supervisor_config_t {
    using duration_t = std::chrono::milliseconds;
    duration_t shutdown_timeout;
    wxEvtHandler *handler;
};

} // namespace wx
} // namespace rotor
