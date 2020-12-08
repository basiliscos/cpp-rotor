#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

/** \file thread.hpp
 * A convenience header to include rotor support for pure thread backends
 */

#include "rotor/thread/supervisor_thread.h"
#include "rotor/thread/system_context_thread.h"

namespace rotor {

/// namespace for pure thread backend (supervisor) for `rotor`
namespace thread {}

} // namespace rotor
