#pragma once

//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
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
#include <boost/lockfree/queue.hpp>

namespace rotor {
namespace thread {

struct supervisor_thread_t;

/** \brief intrusive pointer for thread supervisor */
using supervisor_ptr_t = intrusive_ptr_t<supervisor_thread_t>;

/** \struct system_context_thread_t
 *  \brief The thread system context, for blocking operations
 *
 */
struct system_context_thread_t : public system_context_t {
    /** \brief constructs thread system context
     *
     *  \param queue_size defines pre-allocated ibound queue size for messages from other threads
     *
     *  \param poll_time how much time it will spend in polling inbound queue before switching into
     *  notification mode (i.e. cv + mutex)
     */
    system_context_thread_t(size_t queue_size = 64, pt::time_duration poll_time = pt::millisec{1}) noexcept;
    ~system_context_thread_t();

    /** \brief invokes blocking execution of the supervisor
     *
     * It blocks until root supervisor shuts down.
     *
     */
    virtual void run() noexcept;

    /** \brief checks for messages from external threads and fires expired timers */
    void check() noexcept;

  protected:
    /** \brief an alias for monotonic clock */
    using clock_t = std::chrono::steady_clock;

    /** \struct deadline_info_t
     *  \brief struct to keep timer handlers
     */
    struct deadline_info_t {
        /** \brief non-owning pointer to timer handler  */
        timer_handler_base_t *handler;

        /** \brief time point, after which the timer is considered expired */
        clock_t::time_point deadline;
    };

    /** \brief ordered list of deadline infos (type) */
    using list_t = std::list<deadline_info_t>;

    /** \brief lock-free queue for inbound messages */
    using inbound_queue_t = boost::lockfree::queue<message_base_t *>;

    /** \brief fires handlers for expired timers */
    void update_time() noexcept;

    /** \brief start timer implementation */
    void start_timer(const pt::time_duration &interval, timer_handler_base_t &handler) noexcept;

    /** \brief cancel timer implementation */

    void cancel_timer(request_id_t timer_id) noexcept;

    /** \brief how much time actively poll inbound queue (100% cpu usage), before switching into notify mode */
    pt::time_duration poll_time;

    /** \brief queue for keeping external messages, from other threads/loops/backends */
    inbound_queue_t inbound;

    /** \brief mutex for inbound queue */
    std::mutex mutex;

    /** \brief cv for notifying about pushing messages into inbound queue */
    std::condition_variable cv;

    /** \brief current time */
    clock_t::time_point now;

    /** \brief ordered list of deadline infos */
    list_t timer_nodes;

    /** \brief whether the context is intercepting blocking (I/O) handler */
    bool intercepting = false;

    friend struct supervisor_thread_t;
};

/** \brief intrusive pointer type for system context thread context */
using system_context_ptr_t = rotor::intrusive_ptr_t<system_context_thread_t>;

} // namespace thread
} // namespace rotor
