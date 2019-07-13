#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "arc.hpp"
#include "address.hpp"
#include <typeindex>

namespace rotor {

/** \struct message_base_t
 *  \brief Base class for `rotor` message.
 *
 *  The base class contains destinanation address (in the form of intrusive
 *  pointer to `address_t`) and possibility to detect final message type.
 *
 * The actual message payload meant to be provided by derived classes
 *
 */

struct message_base_t : public arc_base_t<message_base_t> {
    virtual ~message_base_t();

    /**
    * \brief returns unique message type pointer.
    *
    * The unique message type pointer is used to runtime check message type
    *  match when the message is delivered to subscribers.
    *
    */
    virtual const void *get_type_index() const noexcept = 0;

    /** \brief constructor which takes destination address */
    message_base_t(const address_ptr_t &addr) : address{addr} {}

    /** \brief message destination address */
    address_ptr_t address;
};

inline message_base_t::~message_base_t() {}

/** \struct message_t
 *  \brief the generic message meant to hold user-specific payload
 *  \tparam T payload type
 */
template <typename T> struct message_t : public message_base_t {
    /** \brief forwards `args` for payload construction */
    template <typename... Args>
    message_t(const address_ptr_t &addr, Args... args) : message_base_t{addr}, payload{std::forward<Args>(args)...} {}

    virtual const void *get_type_index() const noexcept { return message_type; }

    /** \brief user-defined payload */
    T payload;

    /** \brief static type which uniquely identifies payload-type specialized `message_t` */
    static const void *message_type;
};
using message_ptr_t = intrusive_ptr_t<message_base_t>;

template <typename T> const void *message_t<T>::message_type = static_cast<const void *>(typeid(message_t<T>).name());

} // namespace rotor
