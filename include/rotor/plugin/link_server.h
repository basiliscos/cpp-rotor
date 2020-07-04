#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin.h"
#include <string>
#include <unordered_set>
#include <functional>

namespace rotor::internal {

struct link_server_plugin_t : public plugin_t {
    using linked_clients_t = std::unordered_set<address_ptr_t>;

    using plugin_t::plugin_t;
    static const void *class_identity;
    const void *identity() const noexcept override;

    void activate(actor_base_t *actor) noexcept override;

    virtual void on_link_request(message::link_request_t &message) noexcept;
    virtual void on_unlink_response(message::unlink_response_t &message) noexcept;
    virtual void on_unlink_notify(message::unlink_notify_t &message) noexcept;

    bool handle_shutdown(message::shutdown_request_t *message) noexcept override;

  protected:
    linked_clients_t linked_clients;
};

} // namespace rotor::internal
