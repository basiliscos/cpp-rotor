#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

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
