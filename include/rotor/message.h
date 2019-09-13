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
     * \brief unique message type pointer.
     *
     * The unique message type pointer is used to runtime check message type
     *  match when the message is delivered to subscribers.
     *
     */
    const void *type_index;

    /** \brief message destination address */
    address_ptr_t address;

    /** \brief constructor which takes destination address */
    message_base_t(const void *type_index_, const address_ptr_t &addr) : type_index{type_index_}, address{addr} {}
};

inline message_base_t::~message_base_t() {}

/** \struct message_t
 *  \brief the generic message meant to hold user-specific payload
 *  \tparam T payload type
 */
template <typename T> struct message_t : public message_base_t {

    /** \brief alias for payload type */
    using payload_t = T;

    /** \brief forwards `args` for payload construction */
    template <typename... Args>
    message_t(const address_ptr_t &addr, Args... args)
        : message_base_t{message_type, addr}, payload{std::forward<Args>(args)...} {}

    /** \brief user-defined payload */
    T payload;

    /** \brief static type which uniquely identifies payload-type specialized `message_t` */
    static const void *message_type;
};

/** \brief intrusive pointer for message */
using message_ptr_t = intrusive_ptr_t<message_base_t>;

template <typename T> const void *message_t<T>::message_type = static_cast<const void *>(typeid(message_t<T>).name());

/** \brief constucts message by constructing it's payload; intrusive pointer for the message is returned */
template <typename M, typename... Args> auto make_message(const address_ptr_t &addr, Args &&... args) -> message_ptr_t {
    return message_ptr_t{new message_t<M>(addr, std::forward<Args>(args)...)};
}

} // namespace rotor
