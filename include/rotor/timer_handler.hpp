#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "forward.hpp"
#include <memory>

namespace rotor {

struct timer_handler_base_t {
    actor_base_t *owner;
    request_id_t request_id;

    timer_handler_base_t(actor_base_t *owner_, request_id_t request_id_) noexcept
        : owner{owner_}, request_id{request_id_} {}

    virtual void trigger(bool cancelled) noexcept = 0;
    virtual inline ~timer_handler_base_t();
};

inline timer_handler_base_t::~timer_handler_base_t() {}

using timer_handler_ptr_t = std::unique_ptr<timer_handler_base_t>;

template <typename Object, typename Method> struct timer_handler_t : timer_handler_base_t {
    Object *object;
    Method method;

    timer_handler_t(actor_base_t *owner_, request_id_t request_id_, Object *object_, Method method_) noexcept
        : timer_handler_base_t{owner_, request_id_}, object{object_}, method{std::forward<Method>(method_)} {}

    void trigger(bool cancelled) noexcept override { ((*object).*method)(request_id, cancelled); }
};

} // namespace rotor
