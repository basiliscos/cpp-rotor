#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/supervisor.h"
#include "rotor/ev/supervisor_config_ev.h"
#include "rotor/ev/system_context_ev.h"
#include "rotor/system_context.h"
#include <ev.h>
#include <mutex>
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
 * execution context (theads), creating sub-supervisors (non-root) will
 * not bring concurrency advantages. It is possible, however, create
 * different event loops and let them be used by different threads;
 * in that case different supervisors, and they will be able to communicate
 * via rotor-messaging.
 *
 */
struct supervisor_ev_t : public supervisor_t {

    struct timer_t : public ev_timer {
        timer_id_t timer_id;
    };

    using timer_ptr_t = std::unique_ptr<timer_t>;

    /** \brief constructs new supervisor from parent supervisor and supervisor config
     *
     * the `parent` supervisor can be `null`
     *
     */
    supervisor_ev_t(supervisor_ev_t *parent, const supervisor_config_ev_t &config);
    ~supervisor_ev_t();

    /** \brief creates an actor by forwaring `args` to it
     *
     * The newly created actor belogs to the ev supervisor / ev event loop
     */
    template <typename Actor, typename... Args>
    intrusive_ptr_t<Actor> create_actor(const pt::time_duration &timeout, Args... args) {
        return make_actor<Actor>(*this, timeout, std::forward<Args>(args)...);
    }

    virtual void start() noexcept override;
    virtual void shutdown() noexcept override;
    virtual void enqueue(message_ptr_t message) noexcept override;
    virtual void start_timer(const pt::time_duration &timeout, timer_id_t timer_id) noexcept override;
    virtual void cancel_timer(timer_id_t timer_id) noexcept override;
    virtual void on_timer_trigger(timer_id_t timer_id) noexcept override;
    virtual void shutdown_finish() noexcept override;

    /** \brief retuns ev-loop associated with the supervisor */
    inline struct ev_loop *get_loop() noexcept { return loop; }

    /** \brief returns pointer to the wx system context */
    inline system_context_ev_t *get_context() noexcept { return static_cast<system_context_ev_t *>(context); }

  protected:
    using timers_map_t = std::unordered_map<timer_id_t, timer_ptr_t>;

    /** \brief EV-specific trampoline function for `on_async` method */
    static void async_cb(EV_P_ ev_async *w, int revents) noexcept;

    /** \brief Process external messages (from inbound queue).
     *
     * Used for moving messages in a thread-safe way for the supervisor
     * from external (inbound) queue into internal queue and do further
     * processing / delivery of the messages.
     *
     */
    virtual void on_async() noexcept;

    struct ev_loop *loop;
    bool loop_ownership;

    /** \brief ev-loop specific thread-safe wake-up notifier for external messages delivery */
    ev_async async_watcher;

    /** \brief mutex for protecting inbound queue and pending flag */
    std::mutex inbound_mutex;

    /** \brief whether refcounter should be decreased
     *
     * Async events are "compressed" by EV, i.e. a few async sygnals can be
     * delivere as one. As we do inc/dec for atomic counter, this might be
     * a problem. So by the flag we are sure, that inc/dec will happen
     * only once.
     *
     */
    bool pending;

    /** \brief inbound messages queue, i.e.the structure to hold messages
     * received from other supervisors / threads
     */
    queue_t inbound;

    timers_map_t timers_map;

    friend struct supervisor_ev_shutdown_t;
};

} // namespace ev
} // namespace rotor
