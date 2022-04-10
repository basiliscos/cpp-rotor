#pragma once

//
// Copyright (c) 2019-2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "arc.hpp"
#include "forward.hpp"

#if defined( _MSC_VER )
#pragma warning(push)
#pragma warning(disable: 4251)
#endif

namespace rotor {

/** \struct address_t
 *  \brief Message subscription and delivery point
 *
 * Address is an abstraction of "point of service", i.e. any actor can send
 * an message to an address, and any actor can subscribe on any address to
 * handle certain kind of messages.
 *
 * An actor has "main" address, and in addition it may have "virtual" private
 * addresses to perform routing messages for the specific methods.
 *
 * Addresses are produced by {@link supervisor_t}, which also is responsible
 * for initial delivery of messages on the address. Address lifetime *should*
 * be no longer then corresponding supervisor lifetime.
 *
 * Addresses are non-copyable and non-moveable. The constructor is private
 * and it is intended to be created by supervisor only.
 *
 */

struct ROTOR_API address_t : public arc_base_t<address_t> {
    /// reference to {@link supervisor_t}, which generated the address
    supervisor_t &supervisor;

    /** \brief runtime label, describing some execution group */
    const void *locality;

    address_t(const address_t &) = delete;
    address_t(address_t &&) = delete;

    /** \brief returns true if two addresses are the same, i.e. are located in the
     * same memory region
     */
    inline bool operator==(const address_t &other) const noexcept { return this == &other; }

    /** \brief compares locality fields of the addresses */
    inline bool same_locality(const address_t &other) const noexcept { return this->locality == other.locality; }

  private:
    friend struct supervisor_t;
    address_t(supervisor_t &sup, const void *locality_) : supervisor{sup}, locality{locality_} {}
};

/** \brief intrusive pointer for address */
using address_ptr_t = intrusive_ptr_t<address_t>;

} // namespace rotor

namespace std {
/** \struct hash<rotor::address_ptr_t>
 *  \brief Hash calculator for address
 */
template <> struct hash<rotor::address_ptr_t> {
    /** \brief Calculates hash for the address */
    inline size_t operator()(const rotor::address_ptr_t &address) const noexcept {
        return reinterpret_cast<size_t>(address.get());
    }
};

} // namespace std

#if defined( _MSC_VER )
#pragma warning(pop)
#endif
