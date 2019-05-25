#pragma once

#include "arc.hpp"
#include <typeindex>

namespace rotor {

struct handler_base_t;

struct message_base_t : public arc_base_t<message_base_t> {
  virtual ~message_base_t() {}
  virtual const std::type_index &get_type_index() const noexcept = 0;
  // virtual void dispatch(handler_base_t*) noexcept = 0;
};

template <typename T> struct message_t : public message_base_t {
  using payload_t = T;
  template <typename... Args>
  message_t(Args... args) : payload{std::forward<Args>(args)...} {}
  virtual ~message_t() {}

  virtual const std::type_index &get_type_index() const noexcept {
    return message_type;
  }

  T payload;
  static const std::type_index message_type; //{typeid (message_t)};
};
using message_ptr_t = intrusive_ptr_t<message_base_t>;

template <typename T>
const std::type_index message_t<T>::message_type = typeid(message_t<T>);

} // namespace rotor
