#pragma once

#include "address.hpp"
#include "messages.hpp"

namespace rotor {

struct supervisor_t;
struct system_context_t;

struct actor_base_t : public arc_base_t<address_t> {
  actor_base_t(supervisor_t &supervisor_) : supervisor{supervisor_} {
    address = new address_t(static_cast<void *>(&supervisor_));
  }

  virtual ~actor_base_t() {}

  virtual void on_initialize(message_t<payload::initialize_actor_t> &) {}

  address_ptr_t get_address() const { return address; }

  template <typename M, typename... Args>
  void send(const address_ptr_t &addr, Args &&... args);

  template <typename Handler> void subscribe(Handler &&h);

protected:
  supervisor_t &supervisor;
  address_ptr_t address;
};

using actor_ptr_t = boost::intrusive_ptr<actor_base_t>;

} // namespace rotor
