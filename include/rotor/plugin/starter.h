#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "subscriber.hpp"

namespace rotor::plugin {

struct starter_plugin_t : subscriber_plugin_t<plugin_t<start_policy_t::late>> {
    using parent_t = subscriber_plugin_t<plugin_t<start_policy_t::late>>;

    static const void *class_identity;
    const void *identity() const noexcept override;

    void activate(actor_base_t *actor) noexcept override;

    bool handle_start(message::start_trigger_t *message) noexcept override;

    processing_result_t handle_subscription(message::subscription_t &message) noexcept override;

    void on_start(message::start_trigger_t &message) noexcept;

  protected:
    void post_configure() noexcept override;
    bool configured_handle_init(message::init_request_t *) noexcept override;
};

} // namespace rotor::plugin
