#pragma once
//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "address.hpp"
#include <system_error>
#include <unordered_set>

namespace rotor {

struct actor_base_t;
struct supervisor_t;

/** \brief the substate of actor, to be tracked by behavior */
enum class behavior_state_t {
    UNKNOWN,
    INIT_STARTED,
    INIT_ENDED,
    SHUTDOWN_STARTED,
    SHUTDOWN_CHILDREN_STARTED,
    SHUTDOWN_CHILDREN_FINISHED,
    UNSUBSCRIPTION_STARTED,
    UNSUBSCRIPTION_FINISHED,
    SHUTDOWN_ENDED,
};

/** \struct actor_behavior_t
 * \brief Actor customization point : default lifetime events and reactions to them
 *
 * The behavior lifetime is bounded to it's actor lifetime. Actor's state is changed
 * from the behavior.
 *
 */
struct actor_behavior_t {
    /** \brief behaviror constructor */
    actor_behavior_t(actor_base_t &actor_) : actor{actor_}, substate{behavior_state_t::UNKNOWN} {}
    virtual ~actor_behavior_t();

    /** \brief sends init-confirmation message (it it was asked) and calls `action_finish_init` */
    virtual void action_confirm_init() noexcept;

    /** \brief invokes `init_finish` actor's method */
    virtual void action_finish_init() noexcept;

    /** \brief trigger actor unsubscription */
    virtual void action_unsubscribe_self() noexcept;

    /** \brief confirms shutdown request (if it was asked) and calls `action_commit_shutdown`  */
    virtual void action_confirm_shutdown() noexcept;

    /** \brief changes actors state to `SHUTTED_DOWN` and calls `action_finish_shutdown` */
    virtual void action_commit_shutdown() noexcept;

    /** \brief invokes `shutdown_finish` actor's method */
    virtual void action_finish_shutdown() noexcept;

    /** \brief event, which triggers initialization actions sequence
     *
     *  - action_confirm_init()
     *  - action_finish_init()
     *
     */
    virtual void on_start_init() noexcept;

    /** \brief event, which triggers shutdown actions sequence
     *
     *  - action_unsubscribe_self()
     *  - (waits unsubscription confirmation)
     *  - action_confirm_shutdown()
     *  - action_commit_shutdown()
     *  - action_finish_shutdown()
     */
    virtual void on_start_shutdown() noexcept;

    /** \brief event, which continues shutdown */
    virtual void on_unsubscription() noexcept;

  protected:
    /** \brief a reference for the led actor */
    actor_base_t &actor;

    /** \brief internal behavior state for housekeeping */
    behavior_state_t substate;
};

/** \struct supervisor_behavior_t
 * \brief supervisor specifict events and default actions
 */
struct supervisor_behavior_t : public actor_behavior_t {
    /** \brief behaviror constructor */
    supervisor_behavior_t(supervisor_t &sup);

    /** \brief triggers shutdown requests on all supervisor's children actors */
    virtual void action_shutdown_children() noexcept;

    /** \brief event, which triggers shutdown actions sequence
     *  - action_shutdown_children()
     *  - (waits children shutdown confirmation)
     *  - action_unsubscribe_self()
     *  - (waits unsubscription confirmation)
     *  - action_confirm_shutdown()
     *  - action_commit_shutdown()
     *  - action_finish_shutdown()
     */
    virtual void on_start_shutdown() noexcept override;

    /** \brief event which occurs, when all children actors are removed, continue shuddown sequence */
    virtual void on_childen_removed() noexcept;

    /** \brief reaction on child shutdown failure. By default it is treated as fatal
     * and forwared to the system context */
    virtual void on_shutdown_fail(const address_ptr_t &address, const std::error_code &ec) noexcept;

    virtual void on_start_init() noexcept override;

    /** TODO: reaction on child initialization failure. By default the child is asked for shut down */

    virtual void on_init(const address_ptr_t &address, const std::error_code &ec) noexcept;

    virtual void on_create_child(const address_ptr_t &address) noexcept;

    using initializing_actors_t = std::unordered_set<address_ptr_t>;
    initializing_actors_t initializing_actors;
};

} // namespace rotor
