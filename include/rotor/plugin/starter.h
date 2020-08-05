#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin_base.h"

namespace rotor::plugin {

struct starter_plugin_t : public plugin_base_t {
    using plugin_base_t::plugin_base_t;

    static const void *class_identity;
    const void *identity() const noexcept override;

    void activate(actor_base_t *actor) noexcept override;
    void deactivate() noexcept override;

    template <typename Handler> handler_ptr_t subscribe_actor(Handler &&handler) noexcept;
    template <typename Handler> handler_ptr_t subscribe_actor(Handler &&handler, const address_ptr_t &addr) noexcept;

    bool handle_init(message::init_request_t *) noexcept override;
    bool handle_start(message::start_trigger_t *message) noexcept override;

    processing_result_t handle_subscription(message::subscription_t &message) noexcept override;

    void on_start(message::start_trigger_t &message) noexcept;

  private:
    subscription_container_t tracked;
    bool configured = false;
    bool continue_init = false;
};

} // namespace rotor::plugin
