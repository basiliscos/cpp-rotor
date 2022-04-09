#pragma once

//
// Copyright (c) 2019-2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
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
struct ROTOR_API address_maker_plugin_t : public plugin_base_t {
    using plugin_base_t::plugin_base_t;

    /** The plugin unique identity to allow further static_cast'ing*/
    static const void *class_identity;

    const void *identity() const noexcept override;

    void activate(actor_base_t *actor) noexcept override;
    void deactivate() noexcept override;

    /** \brief smart identity setter
     *
     * It can set the actor identity, and optionally append actor's main addres
     * to let it be something like "net::http10 0x7fc0d0013c60"
     *
     */

    void set_identity(std::string_view name, bool append_addr = true) noexcept;

    /** \brief creates additional actor address (on demand)
     *
     * This is just a shortcut method to create_address() of supervisor
     *
     */
    virtual address_ptr_t create_address() noexcept;

    /** generates default identity like "actor 0x7fc0d0013c60" */
    virtual void generate_identity() noexcept;
};

} // namespace rotor::plugin
