#pragma once

//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/supervisor.h"
#include "supervisor_config_asio.h"
#include "system_context_asio.h"
#include "forwarder.hpp"
#include <boost/asio.hpp>
#include <unordered_map>
#include <memory>

namespace rotor {
namespace asio {

namespace asio = boost::asio;
namespace sys = boost::system;

template <typename Actor, typename Handler, typename ArgsCount, typename ErrHandler> struct forwarder_t;

/** \struct supervisor_asio_t
 *
 *  \brief delivers rotor-messages on top of boost asio event loop
 * using `strand` for serialization
 *
 * The boost::asio `strand` guarantees, that handler, which "belongs" to
 * the same `strand` will be executed sequentially. It might be called
 * on different threads, however.
 *
 * The `supervisor_asio_t` uses that advantage to let the messages to
 * the supervisor and it's actors be delivered sequentially.
 *
 * The "sub-supervisors" can be created, but they do not extend the
 * sequential executing context, i.e. in the execution sense the
 * supervisors are independent.
 *
 * When there is need to "uplift" the boost::asio low-level event,
 * into high-level `rotor` message, the {@link forwarder_t} can be used.
 * It uses the `strand` under the hood.
 *
 * If there is need to change some actor's internals from boost::asio
 * handler, the change should be performed in synchronized way, i.e.
 * via `strand`.
 *
 */
struct supervisor_asio_t : public supervisor_t {

    /** \brief injects an alias for supervisor_config_asio_t */
    using config_t = supervisor_config_asio_t;

    /** \brief injects templated supervisor_config_asio_builder_t */
    template <typename Supervisor> using config_builder_t = supervisor_config_asio_builder_t<Supervisor>;

    /** \brief constructs new supervisor from asio supervisor config */
    supervisor_asio_t(supervisor_config_asio_t &config);

    virtual address_ptr_t make_address() noexcept override;

    virtual void start() noexcept override;
    virtual void shutdown() noexcept override;
    virtual void enqueue(message_ptr_t message) noexcept override;
    virtual void shutdown_finish() noexcept override;

    /** \brief an helper for creation {@link forwarder_t} */
    template <typename Handler, typename ErrHandler>
    auto create_forwarder(Handler &&handler, ErrHandler &&err_handler) {
        return forwarder_t{*this, std::move(handler), std::move(err_handler)};
    }

    /** \brief an helper for creation {@link forwarder_t} (no-error handler case) */
    template <typename Handler> auto create_forwarder(Handler &&handler) {
        return forwarder_t{*this, std::move(handler)};
    }

    /** \brief returns exeuction strand */
    inline asio::io_context::strand &get_strand() noexcept { return *strand; }

    /** \brief process queue of messages of locality leader */
    void do_process() noexcept;

  protected:
    /** \struct timer_t
     * \brief boos::asio::deadline_timer with embedded timer handler */
    struct timer_t : public asio::deadline_timer {

        /** \brief non-owning pointer to timer handler */
        timer_handler_base_t *handler;

        /** \brief constructs timer using timer handler and boost asio io_context */
        timer_t(timer_handler_base_t *handler_, asio::io_context &io_context)
            : asio::deadline_timer(io_context), handler{handler_} {}
    };

    /** \brief unique pointer to timer */
    using timer_ptr_t = std::unique_ptr<timer_t>;

    /** \brief timer id to timer pointer mapping type */
    using timers_map_t = std::unordered_map<request_id_t, timer_ptr_t>;

    void do_start_timer(const pt::time_duration &interval, timer_handler_base_t &handler) noexcept override;
    void do_cancel_timer(request_id_t timer_id) noexcept override;

    /** \brief guard type : alias for asio executor_work_guard */
    using guard_t = asio::executor_work_guard<asio::io_context::executor_type>;

    /** \brief alias for a guard */
    using guard_ptr_t = std::unique_ptr<guard_t>;

    /** \brief timer id to timer pointer mapping */
    timers_map_t timers_map;

    /** \brief config for the supervisor */
    supervisor_config_asio_t::strand_ptr_t strand;

    /** \brief guard to control ownership of the io-context */
    guard_ptr_t guard;

  private:
    void invoke_shutdown() noexcept;
};

template <typename Actor> inline boost::asio::io_context::strand &get_strand(Actor &actor) {
    return actor.get_strand();
}

} // namespace asio
} // namespace rotor
