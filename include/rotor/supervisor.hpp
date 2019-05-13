#pragma once

#include "actor.hpp"
#include "message.hpp"

namespace rotor {

struct supervisor_t : public actor_base_t {
public:
  supervisor_t(system_context_t &system_context_)
      : actor_base_t{system_context_} {}

  template <typename M, typename... Args>
  void enqueue(const address_ptr_t &addr, Args &&... args) {
    auto raw_message = new message_t<M>(std::forward<Args>(args)...);
    outbound.emplace_back(addr, raw_message);
  }

private:
  struct item_t {
    address_ptr_t address;
    message_ptr_t message;

  public:
    item_t(const address_ptr_t &address_, message_ptr_t message_)
        : address{address_}, message{message_} {}
  };
  using queue_t = std::vector<item_t>;

  queue_t outbound;
};

using supervisor_ptr_t = boost::intrusive_ptr<supervisor_t>;

} // namespace rotor
