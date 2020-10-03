#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin_base.h"

namespace rotor::plugin {

/** \struct init_shutdown_plugin_t
 *
 * \brief manages actors init and shutdown procedures
 *
 * All actor plugins are polled when init(shutdown) request is received
 * in the direct(reverse) order. When all actors confirmed that they are
 * ready then initialization (shutdown) is confirmed to uplink.
 *
 */
struct init_shutdown_plugin_t : public plugin_base_t {
    using plugin_base_t::plugin_base_t;

    /** The plugin unique identity to allow further static_cast'ing*/
    static const void *class_identity;

    const void *identity() const noexcept override;

    void activate(actor_base_t *actor) noexcept override;

    /** \brief init message handler */
    void on_init(message::init_request_t &message) noexcept;

    /** \brief shutdown message handler */
    void on_shutdown(message::shutdown_request_t &message) noexcept;

    bool handle_shutdown(message::shutdown_request_t *message) noexcept override;
    bool handle_init(message::init_request_t *message) noexcept override;
};

} // namespace rotor::plugin
