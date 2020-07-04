#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin.h"
#include <string>
#include <unordered_map>
#include <functional>

namespace rotor::internal {

struct link_client_plugin_t : public plugin_t {

    enum class state_t { LINKING, OPERATIONAL, UNLINKING };
    using servers_map_t = std::unordered_map<address_ptr_t, state_t>;
    using unlink_reaction_t = std::function<bool(message::unlink_request_t& message)>;

    using plugin_t::plugin_t;
    static const void *class_identity;
    const void *identity() const noexcept override;

    void activate(actor_base_t *actor) noexcept override;
    virtual void link(address_ptr_t& address) noexcept;
    virtual void forget_link(message::unlink_request_t& message) noexcept;

    virtual void on_link_response(message::link_response_t& message) noexcept;
    virtual void on_unlink_request(message::unlink_request_t& message) noexcept;

    bool handle_shutdown(message::shutdown_request_t *message) noexcept override;
    bool handle_init(message::init_request_t *message) noexcept override;

  protected:
    servers_map_t servers_map;
    unlink_reaction_t unlink_reaction;
    bool configured = false;

};

} // namespace rotor::internal
