#pragma once

#include "actor.hpp"
#include "forwarder.hpp"
#include "handler.hpp"
#include "message.hpp"
#include "messages.hpp"
#include "system_context.hpp"
#include <boost/asio.hpp>
#include <cassert>
#include <chrono>
#include <deque>
#include <functional>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

namespace rotor {

namespace asio = boost::asio;

struct supervisor_config_t {
  using duration_t = boost::posix_time::time_duration;
  duration_t shutdown_timeout;
};

struct supervisor_t : public actor_base_t {
public:
  supervisor_t(system_context_t &system_context_,
               const supervisor_config_t &config_)
      : actor_base_t{*this}, system_context{system_context_},
        strand{system_context.io_context}, state{state_t::OPERATIONAL},
        config{config_}, shutdown_timer{system_context.io_context} {
    subscribe(&actor_base_t::on_initialize);
    subscribe(&supervisor_t::on_shutdown);
    subscribe(&supervisor_t::on_shutdown_confirm);
    subscribe(&supervisor_t::on_start);
  }

  supervisor_t(const supervisor_t &) = delete;
  supervisor_t(supervisor_t &&) = delete;

  template <typename M, typename... Args>
  void enqueue(address_ptr_t addr, Args &&... args) {
    auto raw_message = new message_t<M>(std::forward<Args>(args)...);
    outbound.emplace_back(std::move(addr), raw_message);
  }

  template <typename Actor, typename... Args>
  intrusive_ptr_t<Actor> create_actor(Args... args) {
    using wrapper_t = intrusive_ptr_t<Actor>;
    assert((state == state_t::OPERATIONAL) && "supervisor isn't operational");
    auto raw_object = new Actor{*this, std::forward<Args>(args)...};
    subscribe_actor(*raw_object, &actor_base_t::on_initialize);
    subscribe_actor(*raw_object, &actor_base_t::on_shutdown);
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
      self.process();
    });
  }

  void shutdown() {
    auto actor_ptr = actor_ptr_t(this);
    asio::defer(strand, [actor_ptr = std::move(actor_ptr)]() {
      auto &self = static_cast<supervisor_t &>(*actor_ptr);
      self.send<payload::shutdown_request_t>(self.address);
      self.process();
    });
  }

  void on_start(message_t<payload::start_supervisor_t> &) {
    std::cout << "supervisor_t::on_start\n";
  }

  inline system_context_t &get_context() noexcept { return system_context; }

  inline asio::io_context::strand &get_strand() noexcept { return strand; }

  template <typename Handler>
  void subscribe_actor(actor_base_t &actor, Handler &&handler) {
    using final_handler_t = handler_t<Handler>;
    auto address = actor.get_address();
    auto handler_raw = new final_handler_t(actor, std::move(handler));
    auto handler_ptr = handler_ptr_t{handler_raw};

    subscription_queue.emplace_back(
        subscription_request_t{handler_ptr, address});
  }

  void process() {
    using seen_set_t = std::unordered_set<std::size_t>;

    proccess_subscriptions(nullptr);
    proccess_unsubscriptions(nullptr);

    while (outbound.size()) {

      auto &item = outbound.front();

      auto &address = item.address;
      auto &message = item.message;
      bool skip_delivery =
          address->ctx_addr == this && state == state_t::SHUTTED_DOWN;
      if (!skip_delivery) {
        auto range = handler_map.equal_range(item.address);
        auto it = range.first;
        seen_set_t seen_by;
        while (it != range.second) {
          auto &handler = it->second;
          void *ptr = handler->object_ptr;
          bool changed = false;
          if (seen_by.count(handler->hash()) == 0) {
            seen_by.emplace(handler->hash());
            handler->call(message);

            if (!subscription_queue.empty()) {
              changed = proccess_subscriptions(ptr);
            }
            if (!unsubscription_queue.empty()) {
              changed = proccess_unsubscriptions(ptr);
            }
            if (changed) {
              // std::cout << "changes has been detected\n";
              range = handler_map.equal_range(item.address);
              it = range.first;
            }
          }
          if (!changed) {
            ++it;
          }
        }
        // std::cout << "message " << typeid(item.message.get()).name() << " has
        // been delivered " << count << " times\n";
      }
      outbound.pop_front();
    }
  }

protected:
  inline bool proccess_subscriptions(void *tracked_actor) {
    bool changed = false;
    for (auto it : subscription_queue) {
      auto handler_ptr = it.handler;
      auto address = it.address;
      changed = changed || *handler_ptr == tracked_actor;
      reverser_handler_map.emplace(handler_ptr, address);
      handler_map.emplace(address, handler_ptr);
    }
    subscription_queue.clear();
    return changed;
  }

  inline bool proccess_unsubscriptions(void *tracked_actor) {
    bool changed = false;
    for (auto it : unsubscription_queue) {
      auto handler_ptr = it.handler;
      changed = changed || handler_ptr->object_ptr == tracked_actor;
      auto range_outer = reverser_handler_map.equal_range(handler_ptr);
      auto it_outer = range_outer.first;
      while (it_outer != range_outer.second) {
        auto &addr = it_outer->second;
        auto range_inner = handler_map.equal_range(addr);
        auto it_inner = range_inner.first;
        while (it_inner != range_inner.second) {
          if (it_inner->second == handler_ptr) {
            it_inner = handler_map.erase(it_inner);
            range_inner = handler_map.equal_range(addr);
          } else {
            ++it_inner;
          }
        }
        it_outer = reverser_handler_map.erase(it_outer);
      }
    }
    unsubscription_queue.clear();
    return false;
  }

  void unsubscribe_actor(handler_ptr_t &handler_ptr) {
    unsubscription_queue.push_back(unsubscription_request_t{handler_ptr});
  }

  /*
  address_ptr_t make_address() {
    return address_ptr_t{new address_t(static_cast<void *>(this))};
  }
  */

  void on_initialize(message_t<payload::initialize_actor_t> &msg) override {
    auto actor_addr = msg.payload.actor_address;
    if (actor_addr != address) {
      // TODO: forward?
      send<payload::initialize_actor_t>(actor_addr, actor_addr);
    }
  }

  void on_shutdown(message_t<payload::shutdown_request_t> &) override {
    std::cout << "supervisor_t::on_shutdown\n";
    state = state_t::SHUTTING_DOWN;
    for (auto pair : actors_map) {
      auto addr = pair.first;
      send<payload::shutdown_request_t>(addr);
    }
    if (!actors_map.empty()) {
      shutdown_timer.expires_from_now(config.shutdown_timeout);
      auto forwared =
          forwarder_t(*this, &supervisor_t::on_shutdown_timer_trigger,
                      &supervisor_t::on_shutdown_timer_error);
      shutdown_timer.async_wait(forwared);
    } else {
      actor_ptr_t self{this};
      // unsubscribe_actor(self, false);
      send<payload::shutdown_confirmation_t>(supervisor.get_address(), self);
    }
  }

  void on_shutdown_timer_error(const boost::system::error_code &ec) noexcept {
    std::cout << "supervisor_t::on_shutdown_timer_error\n";
    // ...
  }

  void on_shutdown_timer_trigger() noexcept {
    std::cout << "supervisor_t::on_shutdown_timer_trigger\n";
    // ...
  }

  void unsubscribe_actor(const actor_ptr_t &actor, bool remove_actor = true) {
    std::cout << "supervisor_t::unsubscribe_actor\n";
    auto it = handler_map.begin();
    while (it != handler_map.end()) {
      auto &handler = it->second;
      if (it->second->object_ptr == actor.get()) {
        unsubscribe_actor(handler);
        // it = handler_map.erase(it);
      } else {
      }
      ++it;
    }
    if (remove_actor) {
      auto it_actor = actors_map.find(actor->get_address());
      assert(it_actor != actors_map.end());
      actors_map.erase(it_actor);
    }
  }

  void
  on_shutdown_confirm(message_t<payload::shutdown_confirmation_t> &message) {
    std::cout << "supervisor_t::on_shutdown_confirm\n";
    unsubscribe_actor(message.payload.actor);
    if (actors_map.empty()) {
      std::cout << "supervisor_t::on_shutdown_confirm (unsubscribing self)\n";
      // std::cout << "unsubscribing queue " << unsubscription_queue.size() <<
      // "\n";
      state = state_t::SHUTTED_DOWN;
      shutdown_timer.cancel();
      actor_ptr_t self{this};
      // unsubscribe_actor(self, false);
      send<payload::shutdown_confirmation_t>(supervisor.get_address(), self);
    }
  }

  enum class state_t {
    OPERATIONAL,
    SHUTTING_DOWN,
    SHUTTED_DOWN,
  };

  struct item_t {
    address_ptr_t address;
    message_ptr_t message;

    item_t(const address_ptr_t &address_, message_ptr_t message_)
        : address{address_}, message{message_} {}
  };
  struct subscription_request_t {
    handler_ptr_t handler;
    address_ptr_t address;
  };
  struct unsubscription_request_t {
    handler_ptr_t handler;
  };

  using subscription_queue_t = std::deque<subscription_request_t>;
  using unsubscription_queue_t = std::deque<unsubscription_request_t>;
  using queue_t = std::deque<item_t>;
  using handler_map_t = std::unordered_multimap<address_ptr_t, handler_ptr_t>;
  using reverser_handler_map_t =
      std::unordered_multimap<handler_ptr_t, address_ptr_t>;
  using actors_map_t = std::unordered_map<address_ptr_t, actor_ptr_t>;
  using handler_fn_t = void (*actor_base_t::*)(message_base_t *);
  using timer_t = asio::deadline_timer;

  system_context_t &system_context;
  asio::io_context::strand strand;
  queue_t outbound;
  handler_map_t handler_map;
  reverser_handler_map_t reverser_handler_map;
  actors_map_t actors_map;
  state_t state;
  supervisor_config_t config;
  timer_t shutdown_timer;
  subscription_queue_t subscription_queue;
  unsubscription_queue_t unsubscription_queue;
};

using supervisor_ptr_t = intrusive_ptr_t<supervisor_t>;

/* third-party classes implementations */

template <typename Supervisor, typename... Args>
auto system_context_t::create_supervisor(const supervisor_config_t &config,
                                         Args... args)
    -> intrusive_ptr_t<Supervisor> {
  using wrapper_t = intrusive_ptr_t<Supervisor>;
  auto raw_object = new Supervisor{*this, config, std::forward<Args>(args)...};
  /*
  supervisor = supervisor_ptr_t{raw_object};
  auto address = supervisor->get_address();
  supervisor->send<payload::initialize_actor_t>(address, address);
  */
  return wrapper_t{raw_object};
}

template <typename M, typename... Args>
void actor_base_t::send(const address_ptr_t &addr, Args &&... args) {
  supervisor.enqueue<M>(addr, std::forward<Args>(args)...);
}

void actor_base_t::on_shutdown(message_t<payload::shutdown_request_t> &) {
  std::cout << "actor_base_t::on_shutdown()\n";
  auto destination = supervisor.get_address();
  send<payload::shutdown_confirmation_t>(destination, this);
}

template <typename Handler> void actor_base_t::subscribe(Handler &&h) {
  supervisor.subscribe_actor(*this, std::forward<Handler>(h));
}

} // namespace rotor
