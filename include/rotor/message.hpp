#pragma once

#include "arc.hpp"

namespace rotor {

struct message_base_t : public arc_base_t<message_base_t> {
  virtual ~message_base_t() {}
};

template <typename T> struct message_t : public message_base_t {
  template <typename... Args>
  message_t(Args... args) : payload{std::forward<Args>(args)...} {}

  T payload;
};
using message_ptr_t = boost::intrusive_ptr<message_base_t>;

} // namespace rotor
