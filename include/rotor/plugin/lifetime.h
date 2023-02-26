#pragma once

//
// Copyright (c) 2019-2023 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin_base.h"

namespace rotor::plugin {

/** \struct lifetime_plugin_t
 *
 * \brief manages all actor subscriptions (i.e. from plugins or actor itself).
 *
 * The plugin main focus to properly cancel subscriptions, i.e. every address
 * where an actor plugin or the actor itslef was subscribscribed to.
 *
 */
struct ROTOR_API lifetime_plugin_t : public plugin_base_t {
    using plugin_base_t::plugin_base_t;

    /** The plugin unique identity to allow further static_cast'ing*/
    static const std::type_index class_identity;

    const std::type_index &identity() const noexcept override;

    void activate(actor_base_t *actor) noexcept override;
    void deactivate() noexcept override;

    /** \brief records initial subscription information */
    void initate_subscription(const subscription_info_ptr_t &info) noexcept;

    /** \brief alias for unsubscription trigger (see below) */
    void unsubscribe(const handler_ptr_t &h, const address_ptr_t &addr) noexcept;

    /** \brief triggers unsubscription
     *
     * For internal subscriptions it answers with unsubscription
     * confirimation; for foreign subscription it sends external unsubscription
     * request
     */
    void unsubscribe(const subscription_info_ptr_t &info) noexcept;

    /** \brief reaction on subscription
     *
     * It just forwards it to the actor to poll related plugins
     */
    virtual void on_subscription(message::subscription_t &) noexcept;

    /** \brief reaction on unsubscription
     *
     * It just forwards it to the actor to poll related plugins
     */
    virtual void on_unsubscription(message::unsubscription_t &) noexcept;

    /** \brief reaction on external unsubscription
     *
     * It just forwards it to the actor to poll related plugins
     */
    virtual void on_unsubscription_external(message::unsubscription_external_t &) noexcept;

    bool handle_unsubscription(const subscription_point_t &point, bool external) noexcept override;

    bool handle_shutdown(message::shutdown_request_t *message) noexcept override;

    /** \brief generic non-public fields accessor */
    template <typename T> auto &access() noexcept;

  private:
    void unsubscribe() noexcept;

    /** \brief recorded subscription points (i.e. handler/address pairs) */
    subscription_container_t points;

    bool ready_to_shutdown() noexcept;
};

} // namespace rotor::plugin
