#pragma once

//
// Copyright (c) 2019-2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/supervisor.h"
#include "rotor/ev/export.h"
#include "rotor/ev/supervisor_config_ev.h"
#include "rotor/ev/system_context_ev.h"
#include "rotor/system_context.h"
#include <ev.h>
#include <memory>
#include <unordered_map>

namespace rotor {
namespace ev {

/** \struct supervisor_ev_t
 *  \brief delivers rotor-messages on top of libev event loop
 *
 * Basically it uses libev `ev_async` watcher to deliver `rotor` messages
 * on top of ev event loop.
 *
 * Since ev event loop is not thread-safe and do not provide different
 * execution context (threads), creating sub-supervisors (non-root) will
 * not bring concurrency advantages. It is possible, however, create
 * different event loops and let them be used by different threads;
 * in that case different supervisors, and they will be able to communicate
 * via rotor-messaging.
 *
 */
struct ROTOR_EV_API supervisor_ev_t : public supervisor_t {
    /** \brief injects an alias for supervisor_config_ev_t */
    using config_t = supervisor_config_ev_t;

    /** \brief injects templated supervisor_config_ev_builder_t */
    template <typename Supervisor> using config_builder_t = supervisor_config_ev_builder_t<Supervisor>;

    /** \struct timer_t
     * \brief inheritance of ev_timer, which holds rotor `timer_id`
     */
    struct timer_t : public ev_timer {

        /** \brief intrusive pointer to ev supervisor (type) */
        using supervisor_ptr_t = intrusive_ptr_t<supervisor_ev_t>;

        /** \brief non-owning pointer to timer handler */
        timer_handler_base_t *handler;

        /** \brief intrusive pointer to the supervisor */
        supervisor_ptr_t sup;
    };

    /** \brief an alias for unique pointer, holding `timer_t` */
    using timer_ptr_t = std::unique_ptr<timer_t>;

    /** \brief constructs new supervisor from ev supervisor config */
    supervisor_ev_t(supervisor_config_ev_t &config);
    virtual void do_initialize(system_context_t *ctx) noexcept override;
    ~supervisor_ev_t();

    void start() noexcept override;
    void shutdown() noexcept override;
    void enqueue(message_ptr_t message) noexcept override;
    void shutdown_finish() noexcept override;

    /** \brief retuns ev-loop associated with the supervisor */
    inline struct ev_loop *get_loop() noexcept { return loop; }

    /** \brief returns pointer to the ev system context */
    inline system_context_ev_t *get_context() noexcept { return static_cast<system_context_ev_t *>(context); }

    /** \brief generic non-public fields accessor */
    template <typename T> auto &access() noexcept;

  protected:
    /** \brief a type for mapping `timer_id` to timer pointer */
    using timers_map_t = std::unordered_map<request_id_t, timer_ptr_t>;

    /** \brief EV-specific trampoline function for `on_async` method */
    static void async_cb(EV_P_ ev_async *w, int revents) noexcept;

    void do_start_timer(const pt::time_duration &interval, timer_handler_base_t &handler) noexcept override;
    void do_cancel_timer(request_id_t timer_id) noexcept override;

    /** \brief Process external messages (from inbound queue).
     *
     * Used for moving messages in a thread-safe way for the supervisor
     * from external (inbound) queue into internal queue and do further
     * processing / delivery of the messages.
     *
     */
    virtual void on_async() noexcept;

    /** \brief a pointer to EV event loop, copied from config */
    struct ev_loop *loop;

    /** \brief whether loop should be destroyed by supervisor, copied from config */
    bool loop_ownership;

    /** \brief ev-loop specific thread-safe wake-up notifier for external messages delivery */
    ev_async async_watcher;

    /** \brief how much time spend in active inbound queue polling */
    ev_tstamp poll_duration;

    /** \brief timer_id to timer map */
    timers_map_t timers_map;

    friend struct supervisor_ev_shutdown_t;

  private:
    void move_inbound_queue() noexcept;
};

} // namespace ev
} // namespace rotor
