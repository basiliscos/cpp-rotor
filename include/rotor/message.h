#pragma once

//
// Copyright (c) 2019-2025 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "arc.hpp"
#include "address.hpp"
#include <typeindex>
#include <deque>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

namespace rotor {

struct message_base_t;

/** \struct message_visitor_t
 *  \brief Abstract message visitor interface
 *
 *  As the message type is dynamic and might not be known at compile type,
 *  the `try_visit` method returns `true` or `false` to indicate whether
 *  a message has been successfully processed. If it returns `false`,
 *  that might indicate, that some other message visitor should try a luck.
 *
 *  It should be noted, that it is potentially slow (not very performant)
 *  as it involves multiple visitors and multiple message try attemps
 *  (the double dispatch visitor pattern is not available).
 *
 */
struct ROTOR_API message_visitor_t {
    virtual ~message_visitor_t() = default;

    /** \brief returns `true` if a message has been successfully processed */
    virtual bool try_visit(const message_base_t &message, void *context) const = 0;
};

/** \struct message_base_t
 *  \brief Base class for `rotor` message.
 *
 *  The base class contains destination address (in the form of intrusive
 *  pointer to `address_t`) and possibility to detect final message type.
 *
 * The actual message payload meant to be provided by derived classes
 *
 */
struct message_base_t : public arc_base_t<message_base_t> {
    virtual ~message_base_t() = default;

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

    /** \brief post-delivery destination address, see `make_routed_message()` for usage */
    address_ptr_t next_route;

    /** \brief constructor which takes destination address */
    inline message_base_t(const void *type_index_, const address_ptr_t &addr)
        : type_index(type_index_), address{addr} {}
};

namespace message_support {
ROTOR_API const void *register_type(const std::type_index &type_index) noexcept;
}

/** \struct message_t
 *  \brief the generic message meant to hold user-specific payload
 *  \tparam T payload type
 */
template <typename T> struct message_t : public message_base_t {

    /** \brief alias for payload type */
    using payload_t = T;

    /** \brief struct visitor_t  concrete message type visitor
     *
     * The `visitor_t` class is intentionally not related to
     * `message_visitor_t`, as there is no `try_visit`,
     * because concrete message visitor always "successfully"
     * visits concrete message type.
     *
     */
    struct ROTOR_API visitor_t {
        virtual ~visitor_t() = default;

        /** \brief visit concrete message */
        virtual void on(const message_t &, void *) {}
    };

    /** \brief forwards `args` for payload construction */
    template <typename... Args>
    message_t(const address_ptr_t &addr, Args &&...args)
        : message_base_t{message_type, addr}, payload{std::forward<Args>(args)...} {}

    /** \brief user-defined payload */
    T payload;

    /** \brief unique per-message-type pointer used for routing */
    static const void *message_type;
};

template <typename T> const void *message_t<T>::message_type = message_support::register_type(typeid(message_t<T>));

/** \brief intrusive pointer for message */
using message_ptr_t = intrusive_ptr_t<message_base_t>;

/** \brief structure to hold messages (intrusive pointers) */
using messages_queue_t = std::deque<message_ptr_t>;

/** \brief constructs message by constructing it's payload; intrusive pointer for the message is returned */
template <typename M, typename... Args> auto make_message(const address_ptr_t &addr, Args &&...args) -> message_ptr_t {
    assert(addr);
    return message_ptr_t{new message_t<M>(addr, std::forward<Args>(args)...)};
}

/** \brief constructs message by constructing it's payload; after delivery to destination address
 *  (to all subscribers), the message is routed the specified route_addr; intrusive pointer for the message is returned
 *
 *  The function is used for "synchronized" message post-processing, i.e. once a message has been delivered
 *  and processed by all recipients (can be zero), then it is routed to the specifed address to do cleanup.
 *
 *  Example:
 *  1. an db-actor and opens transaction and reads data (i.e. as `std::string_view`s, owned by db)
 *  2. actors sends broadcast message to all interesting parties to deserialized data
 *  3. **after** the step 2 is finished the db-actor closes transaction and releases acquired resources.
 *
 *  The `make_routed_message` is needed to perform recipient-agnostic 3rd step.
 *
 *  The alternative is to create a copy (snapshot) of data (i.e. `std::string` instead of `std::string_view`), but that
 *  seems redundant.
 */
template <typename M, typename... Args>
auto make_routed_message(const address_ptr_t &addr, const address_ptr_t &route_addr, Args &&...args) -> message_ptr_t {
    assert(addr);
    auto message = message_ptr_t{new message_t<M>(addr, std::forward<Args>(args)...)};
    message->next_route = route_addr;
    return message;
}

} // namespace rotor

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
