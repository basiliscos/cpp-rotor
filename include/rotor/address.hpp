#pragma once

#include "arc.hpp"

namespace rotor {

struct actor_base_t;
struct supervisor_t;

struct address_t : public arc_base_t<address_t> {
  const void *ctx_addr;
  address_t(const address_t &) = delete;
  address_t(address_t &&) = delete;

private:
  friend struct supervisor_t;
  friend struct actor_base_t;
  address_t(void *ctx_addr_) : ctx_addr{ctx_addr_} {}
  inline bool operator==(const address_t &other) const {
    return this == &other;
  }
};
using address_ptr_t = intrusive_ptr_t<address_t>;

} // namespace rotor

namespace std {
template <> struct hash<rotor::address_ptr_t> {
  inline size_t operator()(const rotor::address_ptr_t &address) const noexcept {
    return reinterpret_cast<size_t>(address->ctx_addr);
  }
};

} // namespace std
