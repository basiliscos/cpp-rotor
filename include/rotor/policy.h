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

enum class restart_policy_t {
    always,
    never,
    normal_only,
    fail_only,
};


} // namespace rotor
