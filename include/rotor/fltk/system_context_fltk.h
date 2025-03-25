#pragma once

//
// Copyright (c) 2019-2025 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/arc.hpp"
#include "rotor/fltk/export.h"
#include "rotor/system_context.h"

namespace rotor {
namespace fltk {

struct supervisor_fltk_t;

/** \struct system_context_fltk_t
 *  \brief The FLTK system context to allow rotor messaging with fltk-backend.
 */
struct ROTOR_FLTK_API system_context_fltk_t : system_context_t {
    using system_context_t::system_context_t;

  private:
    void enqueue(message_ptr_t message) noexcept;
    friend supervisor_fltk_t;
};

/** \brief intrusive pointer type for fltk system context */
using system_context_ptr_t = rotor::intrusive_ptr_t<system_context_fltk_t>;

} // namespace fltk
} // namespace rotor
