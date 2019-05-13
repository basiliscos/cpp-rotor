#pragma once

#include "address.hpp"

struct system_context_t;

namespace rotor {

struct actor_base_t : public arc_base_t<address_t> {
  actor_base_t(system_context_t &system_context_)
      : system_context{system_context_} {
    address = new address_t(static_cast<void *>(&system_context));
  }

  address_ptr_t get_address() const { return address; }

private:
  system_context_t &system_context;
  address_ptr_t address;
};

} // namespace rotor
