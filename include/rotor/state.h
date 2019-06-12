#pragma once

namespace rotor {

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
