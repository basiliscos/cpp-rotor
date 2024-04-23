#pragma once

//
// Copyright (c) 2019-2024 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/arc.hpp"
#include "rotor/system_context.h"

namespace rotor {
namespace fltk {

/** \brief alias for system main system context */
using system_context_fltk_t = system_context_t;

/** \brief intrusive pointer type for fltk system context */
using system_context_ptr_t = rotor::intrusive_ptr_t<system_context_fltk_t>;

} // namespace fltk
} // namespace rotor
