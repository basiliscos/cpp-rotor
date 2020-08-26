#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin_base.h"

namespace rotor::plugin {

/** \struct address_maker_plugin_t
 *
 * \brief create actor's addresses
 *
 * The plugin is executed on very early stage of actor creation
 * to assing its main address as soon as possible.
 *
 * If additional addresses are needed by the actor, they can be
 * asked via the plugin.
 *
 */
struct address_maker_plugin_t : public plugin_base_t {
    using plugin_base_t::plugin_base_t;

    static const void *class_identity;
    const void *identity() const noexcept override;

    void activate(actor_base_t *actor) noexcept override;
    void deactivate() noexcept override;

    virtual address_ptr_t create_address() noexcept;
};

} // namespace rotor::plugin
