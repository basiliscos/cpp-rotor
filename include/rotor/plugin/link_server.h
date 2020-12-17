#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin_base.h"
#include <string>
#include <unordered_set>
#include <functional>
#include <optional>

namespace rotor::plugin {

/** \struct link_server_plugin_t
 *
 * \brief allows actor to have passive (server) role in linking
 *
 * The plugin keeps records of connected clients. When actor is
 * going to shutdown it will block shutdown process until all
 * connected clients will send unlink confirmation (or until
 * timeout will trigger).
 *
 */
struct link_server_plugin_t : public plugin_base_t {
    using plugin_base_t::plugin_base_t;

    /** The plugin unique identity to allow further static_cast'ing*/
    static const void *class_identity;

    const void *identity() const noexcept override;

    void activate(actor_base_t *actor) noexcept override;

    /** \brief reaction upon link request */
    virtual void on_link_request(message::link_request_t &message) noexcept;

    /** \brief reaction upon link unlink response */
    virtual void on_unlink_response(message::unlink_response_t &message) noexcept;

    /** \brief reaction upon link unlink notify */
    virtual void on_unlink_notify(message::unlink_notify_t &message) noexcept;

    bool handle_shutdown(message::shutdown_request_t *message) noexcept override;
    void handle_start(message::start_trigger_t *message) noexcept override;

    /** \brief returns true if there is any connected client */
    virtual bool has_clients() noexcept { return !linked_clients.empty(); }

    /** \brief let clients know that the actor is going to shut self down */
    virtual void notify_shutdown() noexcept;

    /** \brief generic non-public fields accessor */
    template <typename T> auto &access() noexcept;

  private:
    enum class link_state_t { PENDING, OPERATIONAL, UNLINKING };
    using link_request_ptr_t = intrusive_ptr_t<message::link_request_t>;
    using request_option_t = std::optional<request_id_t>;
    struct link_info_t {
        link_info_t(link_state_t state_, link_request_ptr_t request_) noexcept : state{state_}, request{request_} {}
        link_state_t state;
        link_request_ptr_t request;
        request_option_t unlink_request;
        bool shutdown_notified = false;
    };

    using linked_clients_t = std::unordered_map<address_ptr_t, link_info_t>;
    linked_clients_t linked_clients;
};

} // namespace rotor::plugin
