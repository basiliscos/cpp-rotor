#pragma once

#include "actor.hpp"
#include "message.hpp"
#include "messages.hpp"
#include "system_context.hpp"
#include <boost/asio.hpp>

namespace rotor {

namespace asio = boost::asio;

struct supervisor_t : public actor_base_t {
public:
  supervisor_t(system_context_t &system_context_)
      : actor_base_t{system_context_}, strand{system_context.io_context} {}

  template <typename M, typename... Args>
  void enqueue(const address_ptr_t &addr, Args &&... args) {
    auto raw_message = new message_t<M>(std::forward<Args>(args)...);
    outbound.emplace_back(addr, raw_message);
  }

  void start() {
      asio::defer(strand, [this]() { this->enqueue<messages::start_supervisor_t>(this->address); });
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

  asio::io_context::strand strand;
  queue_t outbound;
};

using supervisor_ptr_t = boost::intrusive_ptr<supervisor_t>;

template <typename Supervisor, typename... Args>
auto system_context_t::create_supervisor(Args... args) -> boost::intrusive_ptr<Supervisor> {
  using wrapper_t = boost::intrusive_ptr<Supervisor>;
  auto raw_object = new Supervisor{*this, std::forward<Args>(args)...};
  supervisor = supervisor_ptr_t{raw_object};
  return wrapper_t{raw_object};
}


} // namespace rotor
