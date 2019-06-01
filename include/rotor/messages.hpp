#pragma once
#include "address.hpp"
#include "message.h"

namespace rotor {

using actor_ptr_t = intrusive_ptr_t<actor_base_t>;

namespace payload {

// struct start_supervisor_t {};
struct initialize_actor_t {
    address_ptr_t actor_address;
};
struct initialize_confirmation_t {
    address_ptr_t actor_address;
};
struct start_actor_t {};

struct create_actor_t {
    actor_ptr_t actor;
};

struct shutdown_request_t {
    actor_ptr_t actor;
};
struct shutdown_confirmation_t {
    actor_ptr_t actor;
};

} // namespace payload

} // namespace rotor
