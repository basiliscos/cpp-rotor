#pragma once

//
// Copyright (c) 2019-2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin_base.h"

namespace rotor::plugin {

/** \struct foreigners_support_plugin_t
 *
 * \brief allows non-local actors to subscribe on the local addresses of a supervisor.
 */
struct ROTOR_API foreigners_support_plugin_t : public plugin_base_t {
    using plugin_base_t::plugin_base_t;

    /** The plugin unique identity to allow further static_cast'ing*/
    static const void *class_identity;

    const void *identity() const noexcept override;
    void activate(actor_base_t *actor) noexcept override;
    void deactivate() noexcept override;

    /** \brief handler for message call */
    virtual void on_call(message::handler_call_t &message) noexcept;

    /** \brief unsubscription message handler */
    virtual void on_unsubscription(message::commit_unsubscription_t &message) noexcept;

    /** \brief external unsubscription message handler */
    virtual void on_subscription_external(message::external_subscription_t &message) noexcept;

  private:
    subscription_container_t foreign_points;
};

} // namespace rotor::plugin
