#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin.h"
#include <string>
#include <unordered_map>

namespace rotor::internal {

struct registry_plugin_t : public plugin_t {
    using plugin_t::plugin_t;

    enum class state_t { REGISTERING, OPERATIONAL, UNREGISTERING };
    using register_map_t = std::unordered_map<std::string, state_t>;
    using discovery_map_t = std::unordered_map<std::string, address_ptr_t*>;

    static const void *class_identity;
    const void *identity() const noexcept override;

    void activate(actor_base_t *actor) noexcept override;

    virtual void on_registration(message::registration_response_t &) noexcept;
    virtual void on_discovery(message::discovery_response_t &) noexcept;

    virtual bool register_name(const std::string& name, const address_ptr_t& address) noexcept;
    virtual bool discover_name(const std::string& name, address_ptr_t& address) noexcept;

    bool handle_shutdown(message::shutdown_request_t *message) noexcept override;
    bool handle_init(message::init_request_t *message) noexcept override;

protected:
    register_map_t register_map;
    discovery_map_t discovery_map;
    bool configured = false;

    virtual void continue_init(const std::error_code& ec) noexcept;
};

} // namespace rotor::internal
