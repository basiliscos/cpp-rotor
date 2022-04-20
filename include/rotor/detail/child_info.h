#pragma once

//
// Copyright (c) 2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/forward.hpp"
#include "rotor/policy.h"
#include <variant>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

namespace rotor::detail {

enum class shutdown_state_t { none, sent, confirmed };

namespace demand {
struct no {};
struct now {};
struct escalate_failure {};
} // namespace demand

using spawn_demand_t = std::variant<demand::no, demand::now, demand::escalate_failure, pt::time_duration>;

/** \struct child_info_t
 * \brief Child actor runtime housekeeping information.
 *
 * Used by child_manager_plugin_t.
 *
 */
struct ROTOR_API child_info_t : boost::intrusive_ref_counter<child_info_t, boost::thread_unsafe_counter> {
    /** \brief an alias for used microsec clock */
    using clock_t = pt::microsec_clock;

    /** \brief CTOR for spawner-instantiated actor */
    template <typename Factory>
    child_info_t(address_ptr_t address_, Factory &&factory_, restart_policy_t policy_ = restart_policy_t::normal_only,
                 const pt::time_duration &restart_period_ = pt::seconds{15}, size_t max_attempts_ = 0,
                 bool escalate_failure_ = false) noexcept
        : address{std::move(address_)}, factory{std::forward<Factory>(factory_)}, policy{policy_},
          restart_period{restart_period_}, max_attempts{max_attempts_}, escalate_failure{escalate_failure_} {}

    /** \brief CTOR for manually instantiated actor */
    template <typename Factory, typename Actor>
    child_info_t(address_ptr_t address_, Factory &&factory_, Actor &&actor_) noexcept
        : child_info_t(std::move(address_), std::forward<Factory>(factory_)) {
        actor = std::forward<Actor>(actor_);
        if (actor) {
            spawn_attempt();
        }
    }

    /** \brief recalculates the state upon spawn_attempt
     *
     * The last last_instantiation is refreshed, and may be
     * the child info becomes inactivated (due to restart_policy)
     *
     */
    void spawn_attempt() noexcept;

    /** \brief checks whether new actor should be spawned
     *
     * New actor can be spawned right now, a bit later,
     * not spawned at all possibly with triggering supersior
     * shutdown.
     *
     */
    spawn_demand_t next_spawn(bool abnormal_shutdown) noexcept;

    /** \brief actor's address */
    address_ptr_t address;

    /** \brief actor's factory (used by spawner) */
    factory_t factory;

    /** \brief owning reference to current actor instanance */
    actor_ptr_t actor;

    /** \brief restart policy (used by spawner) */
    restart_policy_t policy;

    /** \brief minimum amount of time between spawning attempts */
    pt::time_duration restart_period;

    /** \brief actor shutdown substate */
    shutdown_state_t shutdown = shutdown_state_t::none;

    /** \brief maximum restart attempts (used by spawner) */
    size_t max_attempts;

    /** \brief the current number of restart attempts */
    size_t attempts = 0;

    /** \brief whether the spawner is active */
    bool active = true;

    /** \brief whether an actor failure should be escalated to supersior */
    bool escalate_failure = false;

    /** \brief the timer_id when actor should be spawned */
    request_id_t timer_id{0};

    /** \brief when an actor was successfully spawned */
    pt::ptime last_instantiation;

    /** \brief whether actor is initialized */
    bool initialized = false;

    /** \brief whether actor is started */
    bool started = false;
};

using child_info_ptr_t = boost::intrusive_ptr<child_info_t>;

} // namespace rotor::detail

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
