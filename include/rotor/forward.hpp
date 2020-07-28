#pragma once
//
// Copyright (c) 2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "arc.hpp"
#include <boost/date_time/posix_time/posix_time.hpp>

namespace rotor {

struct address_t;
struct actor_base_t;
struct handler_base_t;
struct plugin_base_t;
struct supervisor_t;
struct system_context_t;

using address_ptr_t = intrusive_ptr_t<address_t>;

/** \brief intrusive pointer for actor*/
using actor_ptr_t = intrusive_ptr_t<actor_base_t>;

/** \brief intrusive pointer for handler */
using handler_ptr_t = intrusive_ptr_t<handler_base_t>;

namespace pt = boost::posix_time;

/** \brief timer identifier type in the scope of the actor */
using request_id_t = std::size_t;

} // namespace rotor
