#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

namespace rotor {

/** \brief state of actor in `rotor` */
enum class state_t {
    UNKNOWN,
    NEW,
    INITIALIZING,
    INITIALIZED,
    OPERATIONAL,
    SHUTTING_DOWN,
    SHUTTED_DOWN,
};

} // namespace rotor
