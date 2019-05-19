#pragma once

#include "actor.hpp"
#include "message.hpp"
#include "messages.hpp"
#include "system_context.hpp"
#include <boost/asio.hpp>
#include <deque>
#include <functional>
#include <iostream>
#include <unordered_map>

namespace rotor {

namespace asio = boost::asio;

template <typename T> struct handler_traits {};
template <typename A, typename M> struct handler_traits<void (A::*)(M &)> {
  using actor_t = A;
  using message_t = M;
  using payload_t = typename message_t::payload_t;
};

struct supervisor_t : public actor_base_t {
public:
  supervisor_t(system_context_t &system_context_)
      : actor_base_t{*this},
        system_context{system_context_}, strand{system_context.io_context} {
    subscribe(&actor_base_t::on_initialize);
    subscribe(&supervisor_t::on_start);
  }

  supervisor_t(const supervisor_t &) = delete;
  supervisor_t(supervisor_t &&) = delete;

  template <typename M, typename... Args>
  void enqueue(const address_ptr_t &addr, Args &&... args) {
    auto raw_message = new message_t<M>(std::forward<Args>(args)...);
    outbound.emplace_back(addr, raw_message);
  }

  template <typename Actor, typename... Args>
  boost::intrusive_ptr<Actor> create_actor(Args... args) {
    using wrapper_t = boost::intrusive_ptr<Actor>;
    auto raw_object = new Actor{*this, std::forward<Args>(args)...};
    subscribe_actor(*raw_object, &actor_base_t::on_initialize);
    auto actor_address = raw_object->get_address();
    actors_map.emplace(actor_address, raw_object);
    send<payload::initialize_actor_t>(address, actor_address);
    return wrapper_t{raw_object};
  }

  void start() {
    auto actor_ptr = actor_ptr_t(this);
    asio::defer(strand, [actor_ptr = std::move(actor_ptr)]() {
      auto &self = static_cast<supervisor_t &>(*actor_ptr);
      self.send<payload::start_supervisor_t>(self.address);
      self.dequeue();
    });
  }

  void on_start(message_t<payload::start_supervisor_t> &) {
    std::cout << "supervisor_t::on_start\n";
  }

  inline system_context_t &get_context() { return system_context; }

  template <typename A, typename Handler>
  void subscribe_actor(A &actor, Handler &&handler) {
    using traits = handler_traits<Handler>;
    using final_message_t = typename traits::message_t;
    using final_actor_t = typename traits::actor_t;
    using payload_t = typename traits::payload_t;

    auto actor_ptr = actor_ptr_t(&actor);
    auto address = actor.get_address();
    handler_t fn = [actor_ptr = std::move(actor_ptr),
                    handler = std::move(handler)](message_ptr_t &msg) mutable {
      auto final_message = dynamic_cast<final_message_t *>(msg.get());
      if (final_message) {
        auto &final_obj = static_cast<final_actor_t &>(*actor_ptr);
        (final_obj.*handler)(*final_message);
      }
    };
    handler_map.emplace(std::move(address), std::move(fn));
  }

protected:
  address_ptr_t make_address() {
    return address_ptr_t{new address_t(static_cast<void *>(this))};
  }

  void on_initialize(message_t<payload::initialize_actor_t> &msg) override {
    auto actor_addr = msg.payload.actor_address;
    if (actor_addr != address) {
      // TODO: forward?
      send<payload::initialize_actor_t>(actor_addr, actor_addr);
    }
  }

  void dequeue() {
    while (outbound.size()) {
      auto &item = outbound.front();
      std::uint32_t count{0};

      auto range = handler_map.equal_range(item.address);
      for (auto it = range.first; it != range.second; ++it) {
        auto &handler = it->second;
        handler(item.message);
        ++count;
      }
      // std::cout << "message " << typeid(item.message.get()).name() << " has
      // been delivered " << count << " times\n";
      outbound.pop_front();
    }
  }

  struct item_t {
    address_ptr_t address;
    message_ptr_t message;

    item_t(const address_ptr_t &address_, message_ptr_t message_)
        : address{address_}, message{message_} {}
  };
  using queue_t = std::deque<item_t>;
  using handler_t = std::function<void(message_ptr_t &)>;
  using handler_map_t = std::unordered_multimap<address_ptr_t, handler_t>;
  using actors_map_t = std::unordered_map<address_ptr_t, actor_ptr_t>;

  system_context_t &system_context;
  asio::io_context::strand strand;
  queue_t outbound;
  handler_map_t handler_map;
  actors_map_t actors_map;
};

using supervisor_ptr_t = boost::intrusive_ptr<supervisor_t>;

/* third-party classes implementations */

template <typename Supervisor, typename... Args>
auto system_context_t::create_supervisor(Args... args)
    -> boost::intrusive_ptr<Supervisor> {
  using wrapper_t = boost::intrusive_ptr<Supervisor>;
  auto raw_object = new Supervisor{*this, std::forward<Args>(args)...};
  supervisor = supervisor_ptr_t{raw_object};
  auto address = supervisor->get_address();
  supervisor->send<payload::initialize_actor_t>(address, address);
  return wrapper_t{raw_object};
}

template <typename M, typename... Args>
void actor_base_t::send(const address_ptr_t &addr, Args &&... args) {
  supervisor.enqueue<M>(addr, std::forward<Args>(args)...);
}

template <typename Handler> void actor_base_t::subscribe(Handler &&h) {
  supervisor.subscribe_actor(*this, std::forward<Handler>(h));
}

} // namespace rotor
