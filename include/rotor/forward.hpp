#pragma once
//
// Copyright (c) 2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "arc.hpp"

namespace rotor {

struct actor_base_t;
struct handler_base_t;
struct supervisor_t;
struct address_t;

using address_ptr_t = intrusive_ptr_t<address_t>;

using actor_ptr_t = intrusive_ptr_t<actor_base_t>;

/** \brief intrusive pointer for handler */
using handler_ptr_t = intrusive_ptr_t<handler_base_t>;

}
