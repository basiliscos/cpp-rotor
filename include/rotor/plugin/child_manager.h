#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin_base.h"
#include <unordered_set>

namespace rotor::plugin {

/** \struct child_manager_plugin_t
 *
 * \brief supervisor's plugin for child-actors housekeeping
 *
 * The plugin is responsible for child actors creation, initialization
 * and shutdown. The supervisor policy is also handled here, e.g.
 * if an child-actor fails to initialize self, the supervisor
 * might shutdown self.
 *
 */
struct child_manager_plugin_t : public plugin_base_t {
    using plugin_base_t::plugin_base_t;

    /** The plugin unique identity to allow further static_cast'ing*/
    static const void *class_identity;

    const void *identity() const noexcept override;

    void activate(actor_base_t *actor) noexcept override;
    void deactivate() noexcept override;

    /** \brief pre-initializes child and sends create_child message to the supervisor */
    virtual void create_child(const actor_ptr_t &actor) noexcept;

    /** \brief removes the child from the supervisor
     *
     * If there was an error, the supervisor might trigger shutdown self (in accordance with policy)
     *
     * Otherwise, if all is ok, it continues the supervisor initialization
     */
    virtual void remove_child(const actor_base_t &actor) noexcept;

    /** \brief reaction on child shutdown failure
     *
     * By default it is forwarded to system context, which terminates program by default
     */
    virtual void on_shutdown_fail(actor_base_t &actor, const std::error_code &ec) noexcept;

    /** \brief sends initialization request upon actor creation message */
    virtual void on_create(message::create_actor_t &message) noexcept;

    /** \brief reaction on (maybe unsuccessful) init confirmatinon
     *
     * Possibilities:
     *  - shutdown child
     *  - shutdown self (supervisor)
     *  - continue init
     *
     */
    virtual void on_init(message::init_response_t &message) noexcept;

    /** \brief reaction on shutdown trigger
     *
     * Possibilities:
     *  - shutdown self (supervisor), via shutdown request
     *  - shutdown self (supervisor) directly, if there is no root supervisor
     *  - shutdown via request for a child
     *
     */
    virtual void on_shutdown_trigger(message::shutdown_trigger_t &message) noexcept;

    /** \brief reacion on shutdown confirmation (i.e. perform some cleanings) */
    virtual void on_shutdown_confirm(message::shutdown_response_t &message) noexcept;

    /** \brief answers about actor's state, identified by it's address
     *
     * If there is no information about the address (including the case when an actor
     * is not yet created or already destroyed), then it replies with `UNKNOWN` status.
     *
     * It replies to the address specified to the `reply_addr` specified in
     * the message {@link payload::state_request_t}.
     *
     */
    virtual void on_state_request(message::state_request_t &message) noexcept;

    bool handle_init(message::init_request_t *) noexcept override;
    bool handle_shutdown(message::shutdown_request_t *) noexcept override;
    void handle_start(message::start_trigger_t *message) noexcept override;

    bool handle_unsubscription(const subscription_point_t &point, bool external) noexcept override;

    /** \brief generic non-public fields accessor */
    template <typename T> auto &access() noexcept;

  private:
    bool has_initializing() const noexcept;
    void init_continue() noexcept;
    void unsubscribe_all(bool continue_shutdown) noexcept;

    struct actor_state_t {
        /** \brief intrusive pointer to actor */
        actor_ptr_t actor;

        template <typename Actor> actor_state_t(Actor &&act) noexcept : actor{std::forward<Actor>(act)} {}

        bool initialized = false;
        bool strated = false;
        /** \brief whether the shutdown request is already sent */
        bool shutdown_requesting = false;
    };

    using actors_map_t = std::unordered_map<address_ptr_t, actor_state_t>;

    /** \brief type for keeping list of initializing actors (during supervisor inititalization) */
    using initializing_actors_t = std::unordered_set<address_ptr_t>;

    bool postponed_init = false;
    /** \brief local address to local actor (intrusive pointer) mapping */
    actors_map_t actors_map;
};

} // namespace rotor::plugin
