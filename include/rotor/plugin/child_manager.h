#pragma once

//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
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
    virtual void on_shutdown_fail(actor_base_t &actor, const extended_error_ptr_t &ec) noexcept;

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

    bool handle_init(message::init_request_t *) noexcept override;
    bool handle_shutdown(message::shutdown_request_t *) noexcept override;
    void handle_start(message::start_trigger_t *message) noexcept override;

    bool handle_unsubscription(const subscription_point_t &point, bool external) noexcept override;

    /** \brief generic non-public fields accessor */
    template <typename T> auto &access() noexcept;

  private:
    enum class request_state_t { NONE, SENT, CONFIRMED };

    struct actor_state_t {
        /** \brief intrusive pointer to actor */
        actor_ptr_t actor;

        template <typename Actor> actor_state_t(Actor &&act) noexcept : actor{std::forward<Actor>(act)} {}

        bool initialized = false;
        bool strated = false;
        request_state_t shutdown = request_state_t::NONE;
    };

    bool has_initializing() const noexcept;
    void init_continue() noexcept;
    void request_shutdown(const extended_error_ptr_t &ec) noexcept;
    void cancel_init(const actor_base_t *child) noexcept;
    void request_shutdown(actor_state_t &actor_state, const extended_error_ptr_t &ec) noexcept;

    using actors_map_t = std::unordered_map<address_ptr_t, actor_state_t>;

    /** \brief type for keeping list of initializing actors (during supervisor inititalization) */
    using initializing_actors_t = std::unordered_set<address_ptr_t>;

    /** \brief local address to local actor (intrusive pointer) mapping */
    actors_map_t actors_map;
};

} // namespace rotor::plugin
