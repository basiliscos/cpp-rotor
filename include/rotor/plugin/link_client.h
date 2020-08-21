#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin_base.h"
#include <string>
#include <unordered_map>
#include <functional>

namespace rotor::plugin {

struct link_client_plugin_t : public plugin_base_t {

    using link_callback_t = std::function<void(const std::error_code &)>;
    using unlink_reaction_t = std::function<bool(message::unlink_request_t &message)>;
    using plugin_base_t::plugin_base_t;

    static const void *class_identity;
    const void *identity() const noexcept override;

    void activate(actor_base_t *actor) noexcept override;
    virtual void link(const address_ptr_t &address, bool operational_only = true,
                      const link_callback_t &callback = {}) noexcept;

    template <typename F> void on_unlink(F &&callback) noexcept { unlink_reaction = std::forward<F>(callback); }

    virtual void on_link_response(message::link_response_t &message) noexcept;
    virtual void on_unlink_request(message::unlink_request_t &message) noexcept;

    bool handle_shutdown(message::shutdown_request_t *message) noexcept override;
    bool handle_init(message::init_request_t *message) noexcept override;

    template <typename T, typename Tag, typename... Args> T access(Args &&...) noexcept;

  private:
    enum class link_state_t { LINKING, OPERATIONAL };
    struct server_record_t {
        link_callback_t callback;
        link_state_t state;
    };
    using servers_map_t = std::unordered_map<address_ptr_t, server_record_t>;

    void forget_link(message::unlink_request_t &message) noexcept;
    servers_map_t servers_map;
    unlink_reaction_t unlink_reaction;
    bool configured = false;
};

} // namespace rotor::plugin
