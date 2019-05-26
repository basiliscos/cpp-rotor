#pragma once

#include "address.hpp"
#include "messages.hpp"
#include <unordered_map>
#include <vector>

namespace rotor {

struct supervisor_t;
struct system_context_t;
struct handler_base_t;
using handler_ptr_t = intrusive_ptr_t<handler_base_t>;

struct actor_base_t : public arc_base_t<actor_base_t> {
  struct subscription_request_t {
    handler_ptr_t handler;
    address_ptr_t address;
  };
  using subscription_points_t =
      std::unordered_map<address_ptr_t, handler_ptr_t>;

  actor_base_t(supervisor_t &supervisor_) : supervisor{supervisor_} {
    auto addr = new address_t(static_cast<void *>(&supervisor_));
    address.reset(addr);
  }

  virtual ~actor_base_t() {}

  virtual void
  on_initialize(message_t<payload::initialize_actor_t> &) noexcept {}
  virtual void on_shutdown(message_t<payload::shutdown_request_t> &) noexcept;

  address_ptr_t get_address() const noexcept { return address; }
  supervisor_t &get_supevisor() const noexcept { return supervisor; }
  subscription_points_t &get_subscription_points() noexcept { return points; }

  template <typename M, typename... Args>
  void send(const address_ptr_t &addr, Args &&... args);

  template <typename Handler> void subscribe(Handler &&h);

  void remember_subscription(const subscription_request_t &req) {
    points.emplace(req.address, req.handler);
  }

  void forget_subscription(const subscription_request_t &req) {
    auto it = points.begin();
    while (it != points.end()) {
      if (it->first == req.address && it->second == req.handler) {
        it = points.erase(it);
      } else {
        ++it;
      }
    }
  }

protected:
  supervisor_t &supervisor;
  address_ptr_t address;
  subscription_points_t points;
};

using actor_ptr_t = intrusive_ptr_t<actor_base_t>;

} // namespace rotor
