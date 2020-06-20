#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin.h"
#include <unordered_set>

namespace rotor::internal {

struct child_manager_plugin_t: public plugin_t {
    using plugin_t::plugin_t;


    /** \brief child actror housekeeping structure */
    struct actor_state_t {
        /** \brief intrusive pointer to actor */
        actor_ptr_t actor;

        /** \brief whethe the shutdown request is already sent */
        bool shutdown_requesting;
    };

    /** \brief (local) address-to-child_actor map type */
    using actors_map_t = std::unordered_map<address_ptr_t, actor_state_t>;

    /** \brief type for keeping list of initializing actors (during supervisor inititalization) */
    using initializing_actors_t = std::unordered_set<address_ptr_t>;

    static const void* class_identity;
    const void* identity() const noexcept override;

    void activate(actor_base_t* actor) noexcept override;
    void deactivate() noexcept override;

    virtual void create_child(const actor_ptr_t &actor) noexcept;
    virtual void remove_child(const actor_base_t &actor, bool normal_flow) noexcept;
    virtual void on_shutdown_fail(actor_base_t &actor, const std::error_code &ec) noexcept;

    virtual void on_create(message::create_actor_t& message) noexcept;
    virtual void on_init(message::init_response_t& message) noexcept;
    virtual void on_shutdown_trigger(message::shutdown_trigger_t& message) noexcept;
    virtual void on_shutdown_confirm(message::shutdown_response_t& message) noexcept;

    /** \brief answers about actor's state, identified by it's address
     *
     * If there is no information about the address (including the case when an actor
     * is not yet created or already destroyed), then it replies with `UNKNOWN` status.
     *
     * It replies to the address specified to the `reply_addr` specified in
     * the message {@link payload::state_request_t}.
     *
     */
    virtual void on_state_request(message::state_request_t& message) noexcept;

    bool handle_init(message::init_request_t*) noexcept override;
    bool handle_shutdown(message::shutdown_request_t*) noexcept override;

    bool handle_unsubscription(const subscription_point_t &point, bool external) noexcept override;

    void unsubscribe_all(bool continue_shutdow) noexcept;

    bool postponed_init = false;
    /** \brief local address to local actor (intrusive pointer) mapping */
    actors_map_t actors_map;

    /** \brief list of initializing actors (during supervisor inititalization) */
    initializing_actors_t initializing_actors;
};

}
