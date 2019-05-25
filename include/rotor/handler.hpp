#pragma once
#include "actor.hpp"
#include "message.hpp"
#include <functional>
#include <memory>
#include <typeindex>
#include <typeinfo>
//#include <iostream>

namespace rotor {

struct actor_base_t;

template <typename T> struct handler_traits {};
template <typename A, typename M> struct handler_traits<void (A::*)(M &)> {
  using actor_t = A;
  using message_t = M;
};

struct handler_base_t : public arc_base_t<handler_base_t> {
  void *object_ptr; /* pointer to actor address */
  handler_base_t(void *object_ptr_) : object_ptr{object_ptr_} {}
  bool operator==(void *ptr) const noexcept { return ptr == object_ptr; }
  virtual bool operator==(const handler_base_t &) const noexcept = 0;
  virtual size_t hash() const noexcept = 0;
  virtual void call(message_ptr_t &) = 0;
  // virtual void op(message_base_t& message) override {

  virtual inline ~handler_base_t() {}
};

using handler_ptr_t = intrusive_ptr_t<handler_base_t>;

template <typename Handler> struct handler_t : public handler_base_t {
  using traits = handler_traits<Handler>;
  using final_message_t = typename traits::message_t;
  using final_actor_t = typename traits::actor_t;

  std::type_index index = typeid(Handler);
  Handler handler;
  std::size_t precalc_hash;
  actor_ptr_t actor_ptr;

  handler_t(actor_base_t &actor, Handler &&handler_)
      : handler_base_t{&actor}, handler{handler_} {
    auto h1 = reinterpret_cast<std::size_t>(static_cast<void *>(object_ptr));
    auto h2 = index.hash_code();
    precalc_hash = h1 ^ (h2 << 1);
    actor_ptr.reset(&actor);
  }
  ~handler_t() {
    // std::cout << "~handler_t for actor " << object_ptr << "\n";
  }

  void call(message_ptr_t &message) override {
    if (message->get_type_index() == final_message_t::message_type) {
      auto final_message = static_cast<final_message_t *>(message.get());
      auto &final_obj = static_cast<final_actor_t &>(*actor_ptr);
      (final_obj.*handler)(*final_message);
    }
  }

  bool operator==(const handler_base_t &rhs) const noexcept override {
    if (object_ptr != rhs.object_ptr) {
      return false;
    }
    auto *other = dynamic_cast<const handler_t *>(&rhs);
    return other && other->handler == handler;
  }

  virtual size_t hash() const noexcept override { return precalc_hash; }
};

/* third-party classes implementations */

/*
template <typename T>
void message_t<T>::dispatch(handler_base_t* handler) noexcept  {
    handler->call(*this);
}
*/

} // namespace rotor

namespace std {
template <> struct hash<rotor::handler_ptr_t> {
  size_t operator()(const rotor::handler_ptr_t &handler) const noexcept {
    return reinterpret_cast<size_t>(handler->hash());
  }
};
} // namespace std
