#pragma once

//
// Copyright (c) 2019-2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin_base.h"
#include <unordered_set>
#include "rotor/detail/child_info.h"

#if defined( _MSC_VER )
#pragma warning(push)
#pragma warning(disable: 4251)
#endif

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
struct ROTOR_API child_manager_plugin_t : public plugin_base_t {
    using plugin_base_t::plugin_base_t;

    /** The plugin unique identity to allow further static_cast'ing*/
    static const void *class_identity;

    const void *identity() const noexcept override;

    void activate(actor_base_t *actor) noexcept override;
    void deactivate() noexcept override;

    /** \brief pre-initializes child and sends create_child message to the supervisor */
    virtual void create_child(const actor_ptr_t &actor) noexcept;

    /** \brief records spawner (via generating a new address) and sends
     * spawn_actor_t message to the supersior */
    virtual void spawn(factory_t factory, const pt::time_duration &period, restart_policy_t policy, size_t max_attempts,
                       bool escalate) noexcept;

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

    /** \brief reaction on shutdown confirmation (i.e. perform some cleanings) */
    virtual void on_shutdown_confirm(message::shutdown_response_t &message) noexcept;

    /** \brief actually attempts to spawn a new actor via spawner */
    virtual void on_spawn(message::spawn_actor_t &message) noexcept;

    bool handle_init(message::init_request_t *) noexcept override;
    bool handle_shutdown(message::shutdown_request_t *) noexcept override;
    void handle_start(message::start_trigger_t *message) noexcept override;

    bool handle_unsubscription(const subscription_point_t &point, bool external) noexcept override;

    /** \brief generic non-public fields accessor */
    template <typename T> auto &access() noexcept;

  private:
    using actors_map_t = std::unordered_map<address_ptr_t, detail::child_info_ptr_t>;
    using spawning_map_t = std::unordered_map<request_id_t, detail::child_info_ptr_t>;

    bool has_initializing() const noexcept;
    void init_continue() noexcept;
    void request_shutdown(const extended_error_ptr_t &ec) noexcept;
    void cancel_init(const actor_base_t *child) noexcept;
    void request_shutdown(detail::child_info_t &child_state, const extended_error_ptr_t &ec) noexcept;
    void on_spawn_timer(request_id_t timer_id, bool cancelled) noexcept;

    size_t active_actors() noexcept;

    /** \brief type for keeping list of initializing actors (during supervisor inititalization) */
    using initializing_actors_t = std::unordered_set<address_ptr_t>;

    /** \brief local address to local actor (intrusive pointer) mapping */
    actors_map_t actors_map;
    spawning_map_t spawning_map;
};

} // namespace rotor::plugin

#if defined( _MSC_VER )
#pragma warning(pop)
#endif
