#pragma once

#include "address.hpp"


namespace rotor {

struct supervisor_t;
struct system_context_t;

system_context_t& get_context(supervisor_t& supervisor);

struct actor_base_t : public arc_base_t<address_t> {
  actor_base_t(supervisor_t &supervisor_)
      : supervisor{supervisor_} {
    address = new address_t(static_cast<void *>(&get_context(supervisor)));
  }

  address_ptr_t get_address() const { return address; }

  template <typename M, typename... Args>
  void send(const address_ptr_t &addr, Args &&... args);

protected:
  supervisor_t& supervisor;
  address_ptr_t address;
};

} // namespace rotor
