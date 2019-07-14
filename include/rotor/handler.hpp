#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "actor_base.h"
#include "message.h"
#include <functional>
#include <memory>
#include <typeindex>
#include <typeinfo>
//#include <iostream>

namespace rotor {

struct actor_base_t;
struct supervisor_t;
using supervisor_ptr_t = intrusive_ptr_t<supervisor_t>;

/** \struct handler_traits
 *  \brief Helper class to extract final actor class and message type from pointer-to-member function
 */
template <typename T> struct handler_traits {};

/** \struct handler_traits<void (A::*)(M &) noexcept>
 *  \brief Helper class to extract final actor class and message type from pointer-to-member function
 */
template <typename A, typename M> struct handler_traits<void (A::*)(M &) noexcept> {
    /** \brief final class of actor */
    using actor_t = A;

    /** \brief message type, processed by the handler */
    using message_t = M;
};

/** \struct handler_base_t
 *  \brief Base class for `rotor` handler, i.e concrete message type processing point
 * on concrete actor
 */
struct handler_base_t : public arc_base_t<handler_base_t> {
    /** \brief pointer to unique message type ( `typeid(Message).name()` ) */
    const void *message_type;

    /** \brief pointer to unique handler type ( `typeid(Handler).name()` ) */
    const void *handler_type;

    /** \brief intrusive poiter to {@link actor_base_t} the actor of the handler */
    actor_ptr_t actor_ptr;

    /** \brief non-owning raw poiter to acto r*/
    const void *raw_actor_ptr;

    /** \brief non-owning raw poiter to acto r*/
    supervisor_t *raw_supervisor_ptr;

    /** \brief precalculated hash for the handler */
    size_t precalc_hash;

    /** \brief constructs `handler_base_t` from raw pointer to actor, raw
     * pointer to message type and raw pointer to handler type
     */
    handler_base_t(actor_base_t &actor, const void *message_type_, const void *handler_type_)
        : message_type{message_type_}, handler_type{handler_type_}, actor_ptr{&actor}, raw_actor_ptr{&actor},
          raw_supervisor_ptr{&actor.get_supervisor()} {
        auto h1 = reinterpret_cast<std::size_t>(handler_type);
        auto h2 = reinterpret_cast<std::size_t>(&actor);
        precalc_hash = h1 ^ (h2 << 1);
    }

    /** \brief compare two handler for equality */
    inline bool operator==(const handler_base_t &rhs) const noexcept {
        return handler_type == rhs.handler_type && raw_actor_ptr == rhs.raw_actor_ptr;
    }

    /** \brief attempt to delivery message to he handler
     *
     * The message is delivered only if its type matches to the handler message type,
     * otherwise it is silently ignored
     */
    virtual void call(message_ptr_t &) noexcept = 0;

    virtual inline ~handler_base_t() {}
};

using handler_ptr_t = intrusive_ptr_t<handler_base_t>;

template <typename Handler> struct handler_t : public handler_base_t {
    /** \brief static pointer to unique pointer-to-member function ( `typeid(Handler).name()` ) */
    static const void *handler_type;

    /** \brief pointer-to-member function instance */
    Handler handler;

    handler_t(actor_base_t &actor, Handler &&handler_)
        : handler_base_t{actor, final_message_t::message_type, handler_type}, handler{handler_} {}

    void call(message_ptr_t &message) noexcept override {
        if (message->get_type_index() == final_message_t::message_type) {
            auto final_message = static_cast<final_message_t *>(message.get());
            auto &final_obj = static_cast<final_actor_t &>(*actor_ptr);
            (final_obj.*handler)(*final_message);
        }
    }

  private:
    using traits = handler_traits<Handler>;
    using final_message_t = typename traits::message_t;
    using final_actor_t = typename traits::actor_t;
};

template <typename Handler>
const void *handler_t<Handler>::handler_type = static_cast<const void *>(typeid(Handler).name());

/* third-party classes implementations */

} // namespace rotor

namespace std {
template <> struct hash<rotor::handler_ptr_t> {
    size_t operator()(const rotor::handler_ptr_t &handler) const noexcept { return handler->precalc_hash; }
};
} // namespace std
