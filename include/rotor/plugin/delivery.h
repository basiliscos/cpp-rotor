#pragma once

//
// Copyright (c) 2019-2023 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin_base.h"
#include "../message_stringifier.h"
#include <string>

#if !defined(NDEBUG) && !defined(ROTOR_DEBUG_DELIVERY)
#define ROTOR_DO_DELIVERY_DEBUG 1
#endif

namespace rotor::plugin {

/** \struct local_delivery_t
 *
 * \brief basic local message delivery implementation
 */
struct ROTOR_API local_delivery_t {

    /** \brief delivers an message for self of one of child-actors  (non-supervisors)
     *
     * Supervisor iterates on subscriptions (handlers) on the message destination address:
     *
     * - If the handler is local (i.e. it's actor belongs to the same supervisor),
     * - Otherwise the message is forwarded for delivery for the foreign supervisor,
     * which owns the handler.
     *
     */

    static void delivery(message_ptr_t &message, const subscription_t::joint_handlers_t &local_recipients) noexcept;
};

/** \struct inspected_local_delivery_t
 *
 * \brief debugging local message delivery implementation with dumping details to stdout.
 */
struct ROTOR_API inspected_local_delivery_t {

    /** \brief delivers the message to the recipients, possibly dumping it to console */
    static void delivery(message_ptr_t &message, const subscription_t::joint_handlers_t &local_recipients,
                         const message_stringifier_t *stringifier) noexcept;

    /** \brief dumps discarded message */
    static void discard(message_ptr_t &message, const message_stringifier_t *stringifier) noexcept;
};

#if !defined(ROTOR_DO_DELIVERY_DEBUG)
using default_local_delivery_t = local_delivery_t;
#else
using default_local_delivery_t = inspected_local_delivery_t;
#endif

/** \struct delivery_plugin_base_t
 *
 * \brief base implementation for messages delivery plugin
 */
struct ROTOR_API delivery_plugin_base_t : public plugin_base_t {
    using plugin_base_t::plugin_base_t;

    /** \brief main messages dispatcher interface */
    virtual size_t process() noexcept = 0;
    void activate(actor_base_t *actor) noexcept override;

  protected:
    /** \brief non-owning raw pointer of supervisor's messages queue */
    messages_queue_t *queue = nullptr;

    /** \brief non-owning raw pointer to supervisor's main address */
    address_t *address = nullptr;

    /** \brief non-owning raw pointer to supervisor's subscriptions map */
    subscription_t *subscription_map;

    /** \brief non-owning raw pointer to system's stringifier */
    const message_stringifier_t *stringifier;
};

/** \brief templated message delivery plugin, to allow local message delivery be customized */
template <typename LocalDelivery = local_delivery_t> struct delivery_plugin_t : public delivery_plugin_base_t {
    using delivery_plugin_base_t::delivery_plugin_base_t;

    /** The plugin unique identity to allow further static_cast'ing*/
    static const std::type_index class_identity;

    const std::type_index &identity() const noexcept override { return class_identity; }

    inline size_t process() noexcept override;
};

template <typename LocalDelivery>
const std::type_index delivery_plugin_t<LocalDelivery>::class_identity = typeid(local_delivery_t);

} // namespace rotor::plugin
