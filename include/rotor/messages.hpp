#pragma once
#include "address.hpp"
#include "message.hpp"

namespace rotor {

using actor_ptr_t = intrusive_ptr_t<actor_base_t>;

namespace payload {

struct start_supervisor_t {};
struct initialize_actor_t {
  address_ptr_t actor_address;
};

struct shutdown_request_t {};
struct shutdown_confirmation_t {
  actor_ptr_t actor;
};

} // namespace payload

} // namespace rotor
