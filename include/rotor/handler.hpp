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

template <typename T> struct handler_traits {};
template <typename A, typename M> struct handler_traits<void (A::*)(M &) noexcept> {
    using actor_t = A;
    using message_t = M;
};

struct handler_base_t : public arc_base_t<handler_base_t> {
    actor_base_t *raw_actor_ptr; /* non-owning pointer to actor address */
    const void *type_index;
    size_t precalc_hash;
    handler_base_t(actor_base_t *actor, const void *type_index_) : raw_actor_ptr{actor}, type_index{type_index_} {
        auto h1 = reinterpret_cast<std::size_t>(static_cast<void *>(raw_actor_ptr));
        auto h2 = reinterpret_cast<std::size_t>(type_index);
        precalc_hash = h1 ^ (h2 << 1);
    }
    bool operator==(actor_base_t *ptr) const noexcept { return ptr == raw_actor_ptr; }
    bool operator==(const handler_base_t &rhs) const noexcept {
        return raw_actor_ptr == rhs.raw_actor_ptr && type_index == rhs.type_index;
    }
    virtual void call(message_ptr_t &) noexcept = 0;

    virtual inline ~handler_base_t() {}
};

using handler_ptr_t = intrusive_ptr_t<handler_base_t>;

template <typename Handler> struct handler_t : public handler_base_t {
    using traits = handler_traits<Handler>;
    using final_message_t = typename traits::message_t;
    using final_actor_t = typename traits::actor_t;

    Handler handler;
    actor_ptr_t actor_ptr;

    handler_t(actor_base_t &actor, Handler &&handler_)
        : handler_base_t{&actor, final_message_t::message_type}, handler{handler_} {
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

/* third-party classes implementations */

} // namespace rotor

namespace std {
template <> struct hash<rotor::handler_ptr_t> {
    size_t operator()(const rotor::handler_ptr_t &handler) const noexcept {
        return reinterpret_cast<size_t>(handler->precalc_hash);
    }
};
} // namespace std
