#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

namespace rotor {

enum class supervisor_policy_t {
    /* shutdown supervisor (and all its actors) if a child-actor them fails during supervisor initialization phase*/
    shutdown_self = 1,

    /* just shutdown failed to initialize child-actor */
    shutdown_failed,

};

}
