#pragma once

#include "address.hpp"
#include "messages.hpp"

namespace rotor {

struct supervisor_t;
struct system_context_t;

struct actor_base_t : public arc_base_t<actor_base_t> {
  actor_base_t(supervisor_t &supervisor_) : supervisor{supervisor_} {
    auto addr = new address_t(static_cast<void *>(&supervisor_));
    // std::cout << "addr created " << (void*)addr << "\n";
    address.reset(addr);
  }

  virtual ~actor_base_t() {
    // std::cout << "~actor_base_t " << (void*)this << "\n";
  }

  virtual void on_initialize(message_t<payload::initialize_actor_t> &) {}
  virtual void on_shutdown(message_t<payload::shutdown_request_t> &);

  address_ptr_t get_address() const noexcept { return address; }
  supervisor_t &get_supevisor() const noexcept { return supervisor; }

  template <typename M, typename... Args>
  void send(const address_ptr_t &addr, Args &&... args);

  template <typename Handler> void subscribe(Handler &&h);

protected:
  supervisor_t &supervisor;
  address_ptr_t address;
};

using actor_ptr_t = intrusive_ptr_t<actor_base_t>;

} // namespace rotor
