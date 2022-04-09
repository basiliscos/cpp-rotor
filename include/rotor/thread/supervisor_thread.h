#pragma once

//
// Copyright (c) 2019-2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/supervisor.h"
#include "system_context_thread.h"

namespace rotor {
namespace thread {

/** \struct supervisor_thread_t
 *  \brief pure thread supervisor dedicated for blocking I/O  or computing operations
 *
 * To let it work, the potentially long blocking I/O operations should
 * be split into smaller chunks, it should be wrapped into message, and actually
 * in the handler the operation should be performed. Lastly, the handler should
 * be tagged I/O.
 */
struct ROTOR_API supervisor_thread_t : public supervisor_t {
    /** \brief constructs new thread supervisor */
    inline supervisor_thread_t(supervisor_config_t &cfg) : supervisor_t{cfg} {}

    void start() noexcept override;
    void shutdown() noexcept override;
    void enqueue(message_ptr_t message) noexcept override;
    void intercept(message_ptr_t &message, const void *tag, const continuation_t &continuation) noexcept override;

    /** \brief updates timer and fires timer handlers, which have been expired */
    void update_time() noexcept;

    void do_start_timer(const pt::time_duration &interval, timer_handler_base_t &handler) noexcept override;
    void do_cancel_timer(request_id_t timer_id) noexcept override;
};

} // namespace thread
} // namespace rotor
