#pragma once
#include "address.hpp"
#include "message.hpp"

namespace rotor {

namespace payload {

struct start_supervisor_t {};
struct initialize_actor_t {
  address_ptr_t actor_address;
};

} // namespace payload

} // namespace rotor
