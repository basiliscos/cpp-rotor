#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/arc.hpp"
#include "rotor/system_context.h"
#include "rotor/timer_handler.hpp"
#include <chrono>
#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>

namespace rotor {
namespace thread {

struct supervisor_thread_t;

/** \brief intrusive pointer for thread supervisor */
using supervisor_ptr_t = intrusive_ptr_t<supervisor_thread_t>;

/** struct system_context_asio_t
 *  brief The thread system context, which holds a reference to
 * .. and root supervisor
 */
struct system_context_thread_t : public system_context_t {
    /* \brief intrusive pointer type for boost::asio system context */
    using ptr_t = rotor::intrusive_ptr_t<system_context_thread_t>;

    /* \brief construct the context from `boost::asio::io_context` reference */
    system_context_thread_t() noexcept;

    virtual void run() noexcept;
    void check() noexcept;

  protected:
    using clock_t = std::chrono::steady_clock;

    struct deadline_info_t {
        timer_handler_base_t *handler;
        clock_t::time_point deadline;
    };
    using list_t = std::list<deadline_info_t>;

    void update_time() noexcept;
    void start_timer(const pt::time_duration &interval, timer_handler_base_t &handler) noexcept;
    void cancel_timer(request_id_t timer_id) noexcept;

    messages_queue_t inbound;
    std::mutex mutex;
    std::condition_variable cv;
    clock_t::time_point now;
    list_t timer_nodes;
    bool intercepting = false;

    friend struct supervisor_thread_t;
};

/* \brief intrusive pointer type for boost::asio system context */
using system_context_ptr_t = rotor::intrusive_ptr_t<system_context_thread_t>;

} // namespace thread
} // namespace rotor
