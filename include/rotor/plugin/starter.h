#pragma once

//
// Copyright (c) 2019-2023 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin_base.h"

namespace rotor::plugin {

/** \struct starter_plugin_t
 *
 * \brief allows custom (actor) subscriptions and it is responsibe
 * for starting actor when it receives {@link message::start_trigger_t}.
 *
 */
struct ROTOR_API starter_plugin_t : public plugin_base_t {
    using plugin_base_t::plugin_base_t;

    /** The plugin unique identity to allow further static_cast'ing*/
    static const std::type_index class_identity;

    const std::type_index &identity() const noexcept override;

    void activate(actor_base_t *actor) noexcept override;
    void deactivate() noexcept override;

    /** \brief subscribes *actor* handler on main actor address */
    template <typename Handler> subscription_info_ptr_t subscribe_actor(Handler &&handler) noexcept;

    /** \brief subscribes *actor* handler on arbitrary address */
    template <typename Handler>
    subscription_info_ptr_t subscribe_actor(Handler &&handler, const address_ptr_t &addr) noexcept;

    bool handle_init(message::init_request_t *) noexcept override;
    void handle_start(message::start_trigger_t *message) noexcept override;
    bool handle_subscription(message::subscription_t &message) noexcept override;

    /** \brief start mesage reaction */
    void on_start(message::start_trigger_t &message) noexcept;

  private:
    subscription_container_t tracked;
    bool configured = false;
};

} // namespace rotor::plugin
