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

    static const void *class_identity;
    const void *identity() const noexcept override;

    void activate(actor_base_t *actor) noexcept override;

    virtual void on_registration(message::registration_response_t &) noexcept;
    virtual void on_discovery(message::discovery_response_t &) noexcept;

    virtual bool register_name(const std::string &name, const address_ptr_t &address) noexcept;
    virtual bool discover_name(const std::string &name, address_ptr_t &address) noexcept;

    bool handle_shutdown(message::shutdown_request_t *message) noexcept override;
    bool handle_init(message::init_request_t *message) noexcept override;

  protected:
    enum class state_t { REGISTERING, OPERATIONAL, UNREGISTERING };
    struct register_info_t {
        address_ptr_t address;
        state_t state;
    };
    using register_map_t = std::unordered_map<std::string, register_info_t>;
    using discovery_map_t = std::unordered_map<std::string, address_ptr_t *>;

    enum plugin_state_t : std::uint32_t {
        CONFIGURED = 1 << 0,
        LINKING = 1 << 1,
        LINKED = 1 << 2,
    };
    std::uint32_t plugin_state = 0;

    register_map_t register_map;
    discovery_map_t discovery_map;

    void link() noexcept;
    void on_link(const std::error_code &ec) noexcept;
    bool has_registering() noexcept;
    virtual void continue_init(const std::error_code &ec) noexcept;
};

} // namespace rotor::internal
