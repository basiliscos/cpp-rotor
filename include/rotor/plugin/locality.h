#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin_base.h"

namespace rotor::plugin {

/** \struct locality_plugin_t
 *
 * \brief detects and assings locality leader to the supevisor
 *
 * For the supervisors hierarchy it detecs top-level supervisor which uses
 * the same locality and assigns it's queue to each of the supervisors
 * in the tree.
 *
 */
struct locality_plugin_t : public plugin_base_t {
    using plugin_base_t::plugin_base_t;

    static const void *class_identity;
    const void *identity() const noexcept override;

    void activate(actor_base_t *actor) noexcept override;
};

} // namespace rotor::plugin
