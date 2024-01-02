#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "forward.hpp"
#include <memory>

namespace rotor {

/** \struct timer_handler_base_t
 *  \brief Base class for timer handler
 */
struct timer_handler_base_t {

    /** \brief actor, which "owns" timer, i.e. where "start_timer" was invoked" */
    actor_base_t *owner;

    /** \brief timer identity (aka timer request id) */
    request_id_t request_id;

    /** \brief constructs timer handler from non-owning pointer to timer and timer request id */
    timer_handler_base_t(actor_base_t *owner_, request_id_t request_id_) noexcept
        : owner{owner_}, request_id{request_id_} {}

    /** \brief an action when timer was triggered or cancelled */
    virtual void trigger(bool cancelled) noexcept = 0;

    virtual ~timer_handler_base_t() = default;
};

/** \brief alias for timer smart pointer */
using timer_handler_ptr_t = std::unique_ptr<timer_handler_base_t>;

/** \struct timer_handler_t
 * \brief templated implementation of timer handler
 *
 */
template <typename Object, typename Method> struct timer_handler_t : timer_handler_base_t {
    /** \brief delegate object, i.e. that one on which the callback (method) will be invoked*/
    Object *object;

    /** \brief timer callback */
    Method method;

    /** \brief constructs timer handler from non-owning pointer to timer, timer request id, delegate and callback
     * (object method) */
    timer_handler_t(actor_base_t *owner_, request_id_t request_id_, Object *object_, Method method_) noexcept
        : timer_handler_base_t{owner_, request_id_}, object{object_}, method{std::forward<Method>(method_)} {}

    void trigger(bool cancelled) noexcept override { ((*object).*method)(request_id, cancelled); }
};

} // namespace rotor
