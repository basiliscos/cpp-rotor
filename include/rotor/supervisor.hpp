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
      : actor_base_t{*this},
        system_context{system_context_}, strand{system_context.io_context} {}

  template <typename M, typename... Args>
  void enqueue(const address_ptr_t &addr, Args &&... args) {
    auto raw_message = new message_t<M>(std::forward<Args>(args)...);
    outbound.emplace_back(addr, raw_message);
  }

  void start() {
    subscribe<payload::start_supervisor_t>(&supervisor_t::on_start);
    asio::defer(strand, [this]() {
      this->send<payload::start_supervisor_t>(this->address);
    });
  }

  void on_start(payload::start_supervisor_t &) {}

  inline system_context_t &get_context() { return system_context; }

  template <typename M, typename Handler>
  void subscribe_actor(const actor_base_t &actor, Handler &&handler) {}

private:
  struct item_t {
    address_ptr_t address;
    message_ptr_t message;

    item_t(const address_ptr_t &address_, message_ptr_t message_)
        : address{address_}, message{message_} {}
  };
  using queue_t = std::vector<item_t>;

  system_context_t &system_context;
  asio::io_context::strand strand;
  queue_t outbound;
};

inline system_context_t &get_context(supervisor_t &supervisor) {
  return supervisor.get_context();
}

using supervisor_ptr_t = boost::intrusive_ptr<supervisor_t>;

/* third-party classes implementations */

template <typename Supervisor, typename... Args>
auto system_context_t::create_supervisor(Args... args)
    -> boost::intrusive_ptr<Supervisor> {
  using wrapper_t = boost::intrusive_ptr<Supervisor>;
  auto raw_object = new Supervisor{*this, std::forward<Args>(args)...};
  supervisor = supervisor_ptr_t{raw_object};
  return wrapper_t{raw_object};
}

template <typename M, typename... Args>
void actor_base_t::send(const address_ptr_t &addr, Args &&... args) {
  supervisor.enqueue<M>(addr, std::forward<Args>(args)...);
}

template <typename M, typename Handler>
void actor_base_t::subscribe(Handler h) {
  supervisor.subscribe_actor<M>(*this, std::move(h));
}

} // namespace rotor
