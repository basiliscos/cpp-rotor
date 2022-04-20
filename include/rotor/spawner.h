#pragma once

//
// Copyright (c) 2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "forward.hpp"
#include "policy.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

namespace rotor {

namespace details {
enum class request_state_t { NONE, SENT, CONFIRMED };
}

/** \struct spawner_t
 * \brief allows automatically restart actors
 *
 * Spawner will NOT create new actor instances when
 * supervisor is SHUTTING_DOWN or SHUT_DOWN.
 *
 */
struct ROTOR_API spawner_t {
    ~spawner_t();

    /** \brief minimum amount of time before respawning new actor
     *
     * Default value is 15 seconds.
     *
     */
    spawner_t &&restart_period(const pt::time_duration &) noexcept;

    /** \brief determines whether new actor instance should be spawned */
    spawner_t &&restart_policy(restart_policy_t) noexcept;

    /** \brief maximum amount of spawned actor instances
     *
     * The default value is `0`, which means try new restart
     * attempt if that is allowed by the policy.
     *
     */
    spawner_t &&max_attempts(size_t) noexcept;

    /** \brief shutdown actor when the spawned actor crashed
     *
     * The actor "crash" means actor terminated with non-zero
     * error code.
     *
     */
    spawner_t &&escalate_failure(bool = true) noexcept;

    /** \brief sends a message to supervisor to spawn
     * the new actor instance
     */
    void spawn() noexcept;

  private:
    spawner_t(factory_t factory, supervisor_t &supervisor) noexcept;

    factory_t factory;
    supervisor_t &supervisor;
    pt::time_duration period = pt::seconds{15};
    restart_policy_t policy = restart_policy_t::normal_only;
    size_t attempts = 0;
    bool done = false;
    bool escalate = false;

    friend struct supervisor_t;
};

} // namespace rotor

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
