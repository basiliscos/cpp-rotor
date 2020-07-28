#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin_base.h"

namespace rotor::plugin {

struct init_shutdown_plugin_t : public plugin_base_t {
    using plugin_base_t::plugin_base_t;

    static const void *class_identity;
    const void *identity() const noexcept override;

    void activate(actor_base_t *actor) noexcept override;

    virtual void on_init(message::init_request_t &message) noexcept;
    virtual void on_shutdown(message::shutdown_request_t &message) noexcept;

    bool handle_shutdown(message::shutdown_request_t *message) noexcept override;
    bool handle_init(message::init_request_t *message) noexcept override;
};

} // namespace rotor::plugin
