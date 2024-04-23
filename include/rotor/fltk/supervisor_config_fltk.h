#pragma once

//
// Copyright (c) 2019-2024 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/supervisor_config.h"

namespace rotor {
namespace fltk {

/** \brief alias for supervisor config */
using supervisor_config_fltk_t = supervisor_config_t;

/** \brief CRTP supervisor fltk config builder (just an alias for generic builder) */
template <typename Supervisor> using supervisor_config_fltk_builder_t = supervisor_config_builder_t<Supervisor>;

} // namespace fltk
} // namespace rotor
