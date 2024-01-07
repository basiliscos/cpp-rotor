#pragma once

//
// Copyright (c) 2019-2023 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin_base.h"

namespace rotor::plugin {

/** \struct locality_plugin_t
 *
 * \brief detects and assigns locality leader to the supervisor
 *
 * For the supervisors hierarchy it detects top-level supervisor which uses
 * the same locality and assigns it's queue to each of the supervisors
 * in the tree.
 *
 */
struct ROTOR_API locality_plugin_t : public plugin_base_t {
    using plugin_base_t::plugin_base_t;

    /** The plugin unique identity to allow further static_cast'ing*/
    static const std::type_index class_identity;

    const std::type_index &identity() const noexcept override;

    void activate(actor_base_t *actor) noexcept override;
};

} // namespace rotor::plugin
