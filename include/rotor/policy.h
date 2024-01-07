#pragma once

//
// Copyright (c) 2019-2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

namespace rotor {

/** \brief how to behave on child actor initialization failures */
enum class supervisor_policy_t {
    /** \brief shutdown supervisor (and all its actors) if a child-actor
     * fails during supervisor initialization phase
     */
    shutdown_self = 1,

    /** \brief shutdown a failed child and continue initialization */
    shutdown_failed,
};

/** \brief spawner's actor restart policy */
enum class restart_policy_t {
    /** \brief always restart actor */
    always,
    /** \brief never restart actor */
    never,

    /** \brief ask terminated actor whether a new instance should be spawned
     * `should_restart()` method is used
     */
    ask_actor,

    /** \brief restart actor only when it terminated normally (without error) */
    normal_only,

    /** \brief restart actor only when it terminated abnormally (with error) */
    fail_only,
};

} // namespace rotor
