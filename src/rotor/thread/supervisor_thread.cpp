//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/thread/supervisor_thread.h"
#include "rotor/thread/system_context_thread.h"

using namespace rotor;
using namespace rotor::thread;

void supervisor_thread_t::start() noexcept {
    // no-op
}

void supervisor_thread_t::shutdown() noexcept {
    auto &sup_addr = supervisor->get_address();
    auto ec = make_error_code(shutdown_code_t::normal);
    auto reason = make_error(ec);
    auto msg = make_message<payload::shutdown_trigger_t>(sup_addr, address, reason);
    supervisor->enqueue(msg);
}

void supervisor_thread_t::enqueue(message_ptr_t message) noexcept {
    auto ctx = static_cast<system_context_thread_t *>(context);
    inbound_queue.push(message.detach());
    std::lock_guard<std::mutex> lock(ctx->mutex);
    ctx->cv.notify_one();
}

void supervisor_thread_t::intercept(message_ptr_t &message, const void *tag,
                                    const continuation_t &continuation) noexcept {
    auto ctx = static_cast<system_context_thread_t *>(context);
    if (tag == rotor::tags::io) {
        ctx->intercepting = true;
        ctx->check();
        supervisor_t::intercept(message, tag, continuation);
        ctx->intercepting = false;
        ctx->check();
    } else {
        supervisor_t::intercept(message, tag, continuation);
    }
}

void supervisor_thread_t::do_start_timer(const pt::time_duration &interval, timer_handler_base_t &handler) noexcept {
    auto ctx = static_cast<system_context_thread_t *>(context);
    ctx->start_timer(interval, handler);
}

void supervisor_thread_t::do_cancel_timer(request_id_t timer_id) noexcept {
    auto ctx = static_cast<system_context_thread_t *>(context);
    ctx->cancel_timer(timer_id);
}

void supervisor_thread_t::update_time() noexcept {
    auto ctx = static_cast<system_context_thread_t *>(context);
    ctx->update_time();
}
