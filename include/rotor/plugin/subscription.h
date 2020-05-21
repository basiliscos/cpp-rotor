#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin.h"

namespace rotor::internal {

struct subscription_plugin_t: public plugin_t {
    using plugin_t::plugin_t;
    using iterator_t = typename subscription_points_t::iterator;

    static const void* class_identity;
    const void* identity() const noexcept override;

    void activate(actor_base_t* actor) noexcept override;
    void deactivate() noexcept override;

    iterator_t find_subscription(const address_ptr_t &addr, const handler_ptr_t &handler) noexcept;

    /* \brief unsubcribes all actor's handlers */
    void unsubscribe() noexcept;

    /** \brief recorded subscription points (i.e. handler/address pairs) */
    subscription_points_t points;

    virtual void on_subscription(message::subscription_t&) noexcept;
    virtual void on_unsubscription(message::unsubscription_t&) noexcept;
    virtual void on_unsubscription_external(message::unsubscription_external_t&) noexcept;

    processing_result_t handle_subscription(message::subscription_t& message) noexcept override;
    processing_result_t handle_unsubscription(message::unsubscription_t& message) noexcept override;
    processing_result_t handle_unsubscription_external(message::unsubscription_external_t& message) noexcept override;

    bool handle_shutdown(message::shutdown_request_t* message) noexcept override;

    processing_result_t remove_subscription(const subscription_point_t& point) noexcept;
};


}
