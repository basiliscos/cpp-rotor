#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/supervisor.h"
#include "supervisor_config.h"
#include "system_context_asio.h"
#include "forwarder.hpp"
#include <boost/asio.hpp>
#include <unordered_map>
#include <memory>

namespace rotor {
namespace asio {

namespace asio = boost::asio;
namespace sys = boost::system;

template <typename Actor, typename Handler, typename ErrHandler> struct forwarder_t;

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
    /** \brief an alias for boos::asio::deadline_timer */

    struct timer_t : public asio::deadline_timer {
        timer_id_t timer_id;

        timer_t(timer_id_t timer_id_, asio::io_context &io_context)
            : asio::deadline_timer(io_context), timer_id{timer_id_} {}
    };

    using timer_ptr_t = std::unique_ptr<timer_t>;
    using timers_map_t = std::unordered_map<std::uint32_t, timer_ptr_t>;

    /** \brief constructs new supervisor from parent supervisor, intrusive
     * pointer to system context and supervisor config and

     * the `parent` supervisor can be null
     *
     */
    supervisor_asio_t(supervisor_t *sup, const pt::time_duration &shutdown_timeout, const supervisor_config_t &config);

    virtual address_ptr_t make_address() noexcept override;

    virtual void start() noexcept override;
    virtual void shutdown() noexcept override;
    virtual void enqueue(message_ptr_t message) noexcept override;
    virtual void start_timer(const pt::time_duration &timeout, timer_id_t timer_id) noexcept override;
    virtual void cancel_timer(timer_id_t timer_id) noexcept override;

    virtual void on_timer_error(timer_id_t timer_id, const sys::error_code &ec) noexcept;

    /** \brief creates an actor by forwaring `args` to it
     *
     * The newly created actor belogs to the current supervisor
     *
     */
    template <typename Actor, typename... Args>
    intrusive_ptr_t<Actor> create_actor(const pt::time_duration &timeout, Args... args) {
        return make_actor<Actor>(*this, timeout, std::forward<Args>(args)...);
    }

    /** \brief an helper for creation {@link forwarder_t} */
    template <typename Handler, typename ErrHandler>
    auto create_forwarder(Handler &&handler, ErrHandler &&err_handler) {
        return forwarder_t{*this, std::move(handler), std::move(err_handler)};
    }

    /** \brief an helper for creation {@link forwarder_t} (no-error handler case) */
    template <typename Handler> auto create_forwarder(Handler &&handler) {
        return forwarder_t{*this, std::move(handler)};
    }

    /** \brief retunrns an reference to boos::asio systerm context
     *
     * It might be useful to get `boost::asio::io_context&`, i.e. to create socket.
     *
     */
    inline system_context_asio_t &get_asio_context() noexcept { return static_cast<system_context_asio_t &>(*context); }

    /** \brief returns exeuction strand */
    inline asio::io_context::strand &get_strand() noexcept { return *config.strand; }

    timers_map_t timers_map;

    /** \brief config for the supervisor */
    supervisor_config_t config;
};

template <typename Actor> inline boost::asio::io_context::strand &get_strand(Actor &actor) {
    return actor.get_strand();
}

} // namespace asio
} // namespace rotor
