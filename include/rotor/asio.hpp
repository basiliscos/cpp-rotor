#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

/** \file asio.hpp
 * A convenience header to include rotor support for boost::asio
 */

#include "rotor/asio/forwarder.hpp"
#include "rotor/asio/supervisor_asio.h"
#include "rotor/asio/supervisor_config_asio.h"
#include "rotor/asio/system_context_asio.h"

namespace rotor {

/// namespace for boost::asio adapters for `rotor`
namespace asio {}

} // namespace rotor
