#pragma once
#include "actor.hpp"
#include "message.hpp"
#include <functional>
#include <memory>
#include <typeindex>
#include <typeinfo>

namespace rotor {

struct actor_base_t;

template <typename T> struct handler_traits {};
template <typename A, typename M> struct handler_traits<void (A::*)(M &)> {
  using actor_t = A;
  using message_t = M;
};

struct handler_base_t : public arc_base_t<handler_base_t> {
  void *object_ptr;
  handler_base_t(void *object_ptr_) : object_ptr{object_ptr_} {}
  virtual bool operator==(const handler_base_t &) noexcept = 0;
  virtual size_t hash() const noexcept = 0;
  virtual void call(message_ptr_t &) = 0;
  virtual inline ~handler_base_t() {}
};

using handler_ptr_t = intrusive_ptr_t<handler_base_t>;

template <typename A, typename Handler>
struct handler_t : public handler_base_t {
  using underlying_fn_t = std::function<void(message_ptr_t &)>;

  std::type_index index = typeid(Handler);
  Handler handler;
  underlying_fn_t fn;

  handler_t(A &actor, Handler &&handler_)
      : handler_base_t{&actor}, handler{handler_} {
    auto actor_ptr = actor_ptr_t{&actor};
    fn = [actor_ptr = actor_ptr,
          handler = std::move(handler)](message_ptr_t &msg) mutable {
      using traits = handler_traits<Handler>;
      using final_message_t = typename traits::message_t;
      using final_actor_t = typename traits::actor_t;

      auto final_message = dynamic_cast<final_message_t *>(msg.get());
      if (final_message) {
        auto &final_obj = static_cast<final_actor_t &>(*actor_ptr);
        (final_obj.*handler)(*final_message);
      }
    };
  }

  void call(message_ptr_t &message) override { fn(message); }

  bool operator==(const handler_base_t &rhs) noexcept override {
    if (object_ptr != rhs.object_ptr) {
      return false;
    }
    auto *other = dynamic_cast<const handler_t *>(&rhs);
    return other && other->handler == handler;
  }

  virtual size_t hash() const noexcept override {
    auto h1 = reinterpret_cast<std::size_t>(static_cast<void *>(object_ptr));
    auto h2 = index.hash_code();
    return h1 ^ (h2 << 1);
  }
};

} // namespace rotor

namespace std {
template <> struct hash<rotor::handler_ptr_t> {
  size_t operator()(const rotor::handler_ptr_t &handler) const noexcept {
    return reinterpret_cast<size_t>(handler->hash());
  }
};
} // namespace std
