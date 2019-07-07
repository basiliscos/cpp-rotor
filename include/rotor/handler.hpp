#pragma once
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

template <typename T> struct handler_traits {};
template <typename A, typename M> struct handler_traits<void (A::*)(M &) noexcept> {
    using actor_t = A;
    using message_t = M;
};

struct handler_base_t : public arc_base_t<handler_base_t> {
    // actor_base_t *raw_actor_ptr; /* non-owning pointer to actor address */
    const void *raw_actor_ptr; /* non-owning pointer to actor address */
    const void *message_type;
    const void *handler_type;
    supervisor_ptr_t supervisor;
    address_ptr_t actor_addr;
    size_t precalc_hash;
    handler_base_t(actor_base_t *actor, const void *message_type_, const void *handler_type_)
        : raw_actor_ptr{actor}, message_type{message_type_}, handler_type{handler_type_},
          supervisor(&actor->get_supervisor()), actor_addr{actor->get_address()} {
        auto h1 = reinterpret_cast<std::size_t>(raw_actor_ptr);
        auto h2 = reinterpret_cast<std::size_t>(handler_type);
        precalc_hash = h1 ^ (h2 << 1);
    }
    inline bool operator==(const handler_base_t &rhs) const noexcept {
        return handler_type == rhs.handler_type && raw_actor_ptr == rhs.raw_actor_ptr;
    }
    virtual void call(message_ptr_t &) noexcept = 0;

    virtual inline ~handler_base_t() {}
};

using handler_ptr_t = intrusive_ptr_t<handler_base_t>;

template <typename Handler> struct handler_t : public handler_base_t {
    using traits = handler_traits<Handler>;
    using final_message_t = typename traits::message_t;
    using final_actor_t = typename traits::actor_t;

    static const void *handler_type;
    Handler handler;
    actor_ptr_t actor_ptr;

    handler_t(actor_base_t &actor, Handler &&handler_)
        : handler_base_t{&actor, final_message_t::message_type, handler_type}, handler{handler_} {
        actor_ptr.reset(&actor);
    }

    void call(message_ptr_t &message) noexcept override {
        if (message->get_type_index() == final_message_t::message_type) {
            auto final_message = static_cast<final_message_t *>(message.get());
            auto &final_obj = static_cast<final_actor_t &>(*actor_ptr);
            (final_obj.*handler)(*final_message);
        }
    }
};

template <typename Handler>
const void *handler_t<Handler>::handler_type = static_cast<const void *>(typeid(Handler).name());

/* third-party classes implementations */

} // namespace rotor

namespace std {
template <> struct hash<rotor::handler_ptr_t> {
    size_t operator()(const rotor::handler_ptr_t &handler) const noexcept {
        return reinterpret_cast<size_t>(handler->precalc_hash);
    }
};
} // namespace std
