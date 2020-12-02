#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/supervisor.h"
#include "system_context_thread.h"

namespace rotor {
namespace thread {

struct supervisor_thread_t : public supervisor_t {
    using supervisor_t::supervisor_t;

    void start() noexcept override;
    void shutdown() noexcept override;
    void enqueue(message_ptr_t message) noexcept override;
    void intercept(message_ptr_t &message, const void *tag, const continuation_t &continuation) noexcept override;

    void do_start_timer(const pt::time_duration &interval, timer_handler_base_t &handler) noexcept override;
    void do_cancel_timer(request_id_t timer_id) noexcept override;
};

} // namespace thread
} // namespace rotor
