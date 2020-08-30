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

/** \struct link_client_plugin_t
 *
 * \brief allows actor to have active (client) role in linking
 *
 * The plugin keeps records of all "servers" where it is connected to.
 * When actor it will notify peers about unlinking.
 *
 * It is allowed to have a custom actors' callback on every link result.
 * connected clients will send unlink confirmation (or until
 * timeout will trigger).
 *
 */
struct link_client_plugin_t : public plugin_base_t {
    /** \brief callback action upon link */
    using link_callback_t = std::function<void(const std::error_code &)>;

    /** \brief unlink interceptor callback
     *
     * if it returns true, the unlink is not performed. It is assumed,
     * that `forget_link` will be called later
     */
    using unlink_reaction_t = std::function<bool(message::unlink_request_t &message)>;

    using plugin_base_t::plugin_base_t;

    /** The plugin unique identity to allow further static_cast'ing*/
    static const void *class_identity;

    const void *identity() const noexcept override;

    void activate(actor_base_t *actor) noexcept override;

    /** \brief attempt to link with the address
     *
     * If `operational_only` is set, then the server side will respond
     * only upon becoming operational.
     *
     * The link callback is always invoked upon response (successful or
     * nor) receiving.
     *
     */
    virtual void link(const address_ptr_t &address, bool operational_only = true,
                      const link_callback_t &callback = {}) noexcept;

    /** \brief sets actor's callback on unlink request */
    template <typename F> void on_unlink(F &&callback) noexcept { unlink_reaction = std::forward<F>(callback); }

    /** \brief handler for link response */
    virtual void on_link_response(message::link_response_t &message) noexcept;

    /** \brief handler for unlink request */
    virtual void on_unlink_request(message::unlink_request_t &message) noexcept;

    bool handle_shutdown(message::shutdown_request_t *message) noexcept override;
    bool handle_init(message::init_request_t *message) noexcept override;

    /** \brief generic non-public fields & methods accessor */
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
