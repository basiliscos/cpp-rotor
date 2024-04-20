#pragma once

//
// Copyright (c) 2019-2024 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/arc.hpp"
#include "rotor/system_context.h"
#include "rotor/fltk/export.h"

namespace rotor {
namespace fltk {

struct supervisor_fltk_t;

struct ROTOR_FLTK_API system_context_fltk_t: system_context_t {
    using system_context_t::system_context_t;

    protected:

    void enqueue_message(supervisor_fltk_t* supervisor, message_ptr_t message) noexcept;

    friend struct supervisor_fltk_t;
};

/** \brief intrusive pointer type for fltk system context */
using system_context_ptr_t = rotor::intrusive_ptr_t<system_context_fltk_t>;

} // namespace fltk
} // namespace rotor
