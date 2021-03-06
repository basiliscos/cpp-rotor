//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/supervisor.h"

using namespace rotor;

namespace {
namespace to {
struct intercept {};
} // namespace to
} // namespace

template <>
inline auto rotor::supervisor_t::access<to::intercept, message_ptr_t &, const void *, const continuation_t &>(
    message_ptr_t &msg, const void *tag, const continuation_t &continuation) noexcept {
    return intercept(msg, tag, continuation);
}

struct continuation_impl_t : continuation_t {
    handler_intercepted_t &handler;
    message_ptr_t &message;

    continuation_impl_t(handler_intercepted_t &handler_, message_ptr_t &message_) noexcept
        : handler{handler_}, message{message_} {}

    void operator()() const noexcept { handler.call_no_check(message); }
};

handler_base_t::handler_base_t(actor_base_t &actor, const void *message_type_, const void *handler_type_) noexcept
    : message_type{message_type_}, handler_type{handler_type_}, actor_ptr{&actor}, raw_actor_ptr{&actor} {
    auto h1 = reinterpret_cast<std::size_t>(handler_type);
    auto h2 = reinterpret_cast<std::size_t>(&actor);
    precalc_hash = h1 ^ (h2 << 1);
}

handler_ptr_t handler_base_t::upgrade(const void *tag) noexcept {
    handler_ptr_t self(this);
    return handler_ptr_t(new handler_intercepted_t(self, tag));
}

handler_intercepted_t::handler_intercepted_t(handler_ptr_t backend_, const void *tag_) noexcept
    : handler_base_t(*backend_->actor_ptr, backend_->message_type, backend_->handler_type), backend{backend_},
      tag{tag_} {}

void handler_intercepted_t::call(message_ptr_t &message) noexcept {
    if (select(message)) {
        auto &sup = actor_ptr->get_supervisor();
        continuation_impl_t continuation(*this, message);
        sup.access<to::intercept, message_ptr_t &, const void *, const continuation_t &>(message, tag, continuation);
    }
}

bool handler_intercepted_t::select(message_ptr_t &message) noexcept { return backend->select(message); }

void handler_intercepted_t::call_no_check(message_ptr_t &message) noexcept { return backend->call_no_check(message); }

handler_ptr_t handler_intercepted_t::upgrade(const void *tag) noexcept {
    if (tag == this->tag) {
        return handler_ptr_t(this);
    }
    return handler_base_t::upgrade(tag);
}
