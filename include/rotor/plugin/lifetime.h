#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin.h"

namespace rotor::internal {

struct lifetime_plugin_t: public plugin_t {
    using plugin_t::plugin_t;

    static const void* class_identity;
    const void* identity() const noexcept override;

    void activate(actor_base_t* actor) noexcept override;
    void deactivate() noexcept override;

    void unsubscribe() noexcept;

    void initate_subscription(const subscription_info_ptr_t& info) noexcept;
    void unsubscribe(const subscription_info_ptr_t& info) noexcept;

    /** \brief recorded subscription points (i.e. handler/address pairs) */
    subscription_container_t points;

    virtual void on_subscription(message::subscription_t&) noexcept;
    virtual void on_unsubscription(message::unsubscription_t&) noexcept;
    virtual void on_unsubscription_external(message::unsubscription_external_t&) noexcept;

    processing_result_t handle_subscription(message::subscription_t& message) noexcept override;
    bool handle_unsubscription(const subscription_point_t& point, bool external) noexcept override;
    //bool handle_unsubscription_external(message::unsubscription_external_t& message) noexcept override;

    bool handle_shutdown(message::shutdown_request_t* message) noexcept override;
};


}