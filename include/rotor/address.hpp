#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "arc.hpp"

namespace rotor {

struct actor_base_t;
struct supervisor_t;

/** \struct address_t
 *  \brief Message subscription and delivery point
 *
 * Address is an abstraction of "point of service", i.e. any actor can send
 * an message to an address, and any actor can subscribe on any address to
 * handle certain kind of messages.
 *
 * Addresses are produced by {@link supervisor_t}, which also is responsible
 * for initial delivery of messages on the address. Address lifetime *should*
 * be no longer then corresponding supervisor lifetime.
 *
 * Addresses are non-copyable and non-moveable. The constructor is private
 * and it is intended to be created by supervisor only.
 *
 */

struct address_t : public arc_base_t<address_t> {
    /// reference to {@link supervisor_t}, which generated the address
    supervisor_t &supervisor;

    address_t(const address_t &) = delete;
    address_t(address_t &&) = delete;

  private:
    friend struct supervisor_t;
    address_t(supervisor_t &sup) : supervisor{sup} {}
    inline bool operator==(const address_t &other) const { return this == &other; }
};

using address_ptr_t = intrusive_ptr_t<address_t>;

} // namespace rotor

namespace std {
template <> struct hash<rotor::address_ptr_t> {
    inline size_t operator()(const rotor::address_ptr_t &address) const noexcept {
        return reinterpret_cast<size_t>(address.get());
    }
};

} // namespace std
