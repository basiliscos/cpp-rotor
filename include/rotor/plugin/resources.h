#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin_base.h"
#include <vector>

namespace rotor::plugin {

using resource_id_t = std::size_t;

/** \struct resources_plugin_t
 *
 * \brief "lock" for external resources
 *
 * The main purpose of the plugin is to let actor know, that some
 * external resources are acquired, and the actor should will be
 * suspended until they will be released.
 *
 * The suspension will happen during init and shutdown phases, e.g.:
 * - actor can wait, until connection will be estableshed
 * - actor can wait, until it receives handshake from remote system
 * - etc...
 *
 * The used `resource_id` can be anything, which has meaning for
 * an actor. Internally `resource_id` is just an index in vector,
 * so don't create it too big.
 *
 */
struct resources_plugin_t : public plugin_base_t {
    using plugin_base_t::plugin_base_t;

    /** The plugin unique identity to allow further static_cast'ing*/
    static const void *class_identity;

    const void *identity() const noexcept override;

    void activate(actor_base_t *actor) noexcept override;
    bool handle_init(message::init_request_t *message) noexcept override;
    bool handle_shutdown(message::shutdown_request_t *message) noexcept override;

    /** \brief increments the resource counter (aka locks)
     *
     * It will block init/shutdown until the resource be released
     *
     */
    virtual void acquire(resource_id_t = 0) noexcept;

    /** \brief decrements the resource counter (aka unlocks)
     *
     * If there is no any resource free, the init/shutdown
     * procedure (if it was started) will be continued
     *
     */
    virtual bool release(resource_id_t = 0) noexcept;

    /** \brief returns counter value for `resource_id`
     *
     * if the resource was freed, zero is returned
     *
     */
    virtual std::uint32_t has(resource_id_t = 0) noexcept;

    /** \brief returns true if there is any resource is locked */
    virtual bool has_any() noexcept;

    /** \brief generic non-public fields accessor */
    template <typename T> auto &access() noexcept;

  private:
    using Resources = std::vector<std::uint32_t>;
    Resources resources;
    bool configured = false;
};

} // namespace rotor::plugin
