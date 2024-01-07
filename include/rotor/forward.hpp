#pragma once
//
// Copyright (c) 2019-2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <functional>
#include "arc.hpp"
#include <boost/date_time/posix_time/posix_time.hpp>

namespace rotor {

struct address_t;
struct actor_base_t;
struct handler_base_t;
struct supervisor_t;
struct system_context_t;

using address_ptr_t = intrusive_ptr_t<address_t>;

/** \brief intrusive pointer for actor*/
using actor_ptr_t = intrusive_ptr_t<actor_base_t>;

/** \brief intrusive pointer for handler */
using handler_ptr_t = intrusive_ptr_t<handler_base_t>;

/** \brief intrusive pointer for supervisor */
using supervisor_ptr_t = intrusive_ptr_t<supervisor_t>;

namespace pt = boost::posix_time;

/** \brief timer identifier type in the scope of the actor */
using request_id_t = std::size_t;

/** \brief factory which allows to create actors lazily or on demand
 *
 * The spawner address MUST be set to the newly created actor to
 * allow further spawning.
 *
 * This function might throw an exception, which is however ignored,
 * but spawner might attempt to create new actor instance.
 *
 */
using factory_t = std::function<actor_ptr_t(supervisor_t &, const address_ptr_t &)>;

} // namespace rotor

namespace rotor::plugin {
struct plugin_base_t;
}
